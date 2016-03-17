#include <sys/socket.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <arpa/inet.h>

#include <iostream>
#include <algorithm>
#include <list>

#define PORT "3100"
#define MAX_EVENTS 32
#define MAX_LEN 1024

int SetNonblock(int fd)
{
    int flags;
#if defined (O_NONBLOCK)
    if( (flags = fcntl(fd, F_GETFL, 0)) == -1) 
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
    flags = 1;
    return ioctl(fd, FIOBIO, &flags);
#endif
}

int main()
{
    struct addrinfo hints, *servinfo;
    bzero(&hints, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, PORT, &hints, &servinfo);
    int master_socket = socket(servinfo->ai_family,
                               servinfo->ai_socktype,
                               servinfo->ai_protocol);
    
    bind(master_socket, servinfo->ai_addr, servinfo->ai_addrlen);
    freeaddrinfo(servinfo);
    int optval = 1;
    setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
    SetNonblock(master_socket);
    listen(master_socket, SOMAXCONN);
    
    int epfd = epoll_create1(0);
    struct epoll_event event_master_socket;
    event_master_socket.data.fd = master_socket;
    event_master_socket.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, master_socket, &event_master_socket);

    while(true) {
        char buffer[MAX_LEN];
        bzero(buffer, MAX_LEN);

        struct epoll_event events_slave_sockets[MAX_EVENTS];
        int ret_ev = epoll_wait(epfd, events_slave_sockets, MAX_EVENTS, -1);
        for(int i = 0; i < ret_ev; ++i) {
            if(events_slave_sockets[i].data.fd == master_socket) {
                int slave_socket = accept(master_socket, NULL, NULL);
                SetNonblock(slave_socket);
                const char *welcome = "Welcome!\n";
                send(slave_socket, welcome, strlen(welcome) + 1, MSG_NOSIGNAL);
                struct epoll_event event_slave_socket;
                event_slave_socket.data.fd = slave_socket;
                event_slave_socket.events = EPOLLIN | EPOLLOUT;
                epoll_ctl(epfd, EPOLL_CTL_ADD, slave_socket, &event_slave_socket);
                std::cout << "accepted connection.." << std::endl;
            } else {
                if(events_slave_sockets[i].events & EPOLLIN) {
                    int recv_res = recv(events_slave_sockets[i].data.fd,
                                        buffer,
                                        MAX_LEN,
                                        0);
                    if(recv_res == 0 && errno != EAGAIN) {
                        shutdown(events_slave_sockets[i].data.fd, SHUT_RDWR);
                        close(events_slave_sockets[i].data.fd);
                        std::cout << "connection terminated.." << std::endl;
                         
                    } else {
                        for(int i = 0; i < ret_ev; ++i) {
                            if(events_slave_sockets[i].events & EPOLLOUT) {
                            send(events_slave_sockets[i].data.fd,
                                 buffer,
                                 MAX_LEN,
                                 MSG_NOSIGNAL);
                            }
                        }
                    }
                }
            }
        }

     }

    return 0;
}
