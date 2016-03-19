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
#include <stdio.h>

#include <iostream>
#include <algorithm>

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

    if(getaddrinfo(NULL, PORT, &hints, &servinfo) != 0) {
        perror("getaddrinfo() error");
        return 1;
    }
    int master_socket = socket(servinfo->ai_family,
                               servinfo->ai_socktype,
                               servinfo->ai_protocol);
    if(master_socket == -1) {
        perror("socket() error");
        return 1;
    }
    
    if(bind(master_socket, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        perror("bind() error");
        return 1;
    }
    freeaddrinfo(servinfo);
    int optval = 1;
    if(setsockopt(master_socket, SOL_SOCKET, 
                  SO_REUSEADDR, &optval, sizeof optval) == -1) {
        perror("setsockopt() error");
    }
    SetNonblock(master_socket);
    if(listen(master_socket, SOMAXCONN) == -1) {
        perror("listen() error");
        return 1;
    }
    
    int epfd = epoll_create1(0);
    struct epoll_event event_master_socket;
    event_master_socket.data.fd = master_socket;
    event_master_socket.events = EPOLLIN;
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, master_socket, &event_master_socket) == -1) {
        perror("epoll_ctl() error");
        return 1;
    }

    while(true) {
        char buffer[MAX_LEN];
        bzero(buffer, MAX_LEN);

        struct epoll_event events_slave_sockets[MAX_EVENTS];
        int ret_ev = epoll_wait(epfd, events_slave_sockets, MAX_EVENTS, -1);
        if(ret_ev == -1) {
            perror("epoll_wait() error");
            break;
        }
        for(int i = 0; i < ret_ev; ++i) {
            if(events_slave_sockets[i].data.fd == master_socket) {
                int slave_socket = accept(master_socket, NULL, NULL);
                if(slave_socket == -1) {
                    perror("accept() error");
                    continue;
                }
                SetNonblock(slave_socket);
                const char *welcome = "Welcome!\n";
                send(slave_socket, welcome, strlen(welcome) + 1, MSG_NOSIGNAL);
                struct epoll_event event_slave_socket;
                event_slave_socket.data.fd = slave_socket;
                event_slave_socket.events = EPOLLIN | EPOLLOUT;
                if(epoll_ctl(epfd, EPOLL_CTL_ADD,
                             slave_socket, &event_slave_socket) == -1) {
                    perror("epoll_ctl() error");
                    close(slave_socket);
                    continue;
                }
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
                                std::string tmp(buffer);
                                tmp.push_back('\n');
                                if(send(events_slave_sockets[i].data.fd,
                                   tmp.c_str(),
                                   tmp.length() + 1,
                                   MSG_NOSIGNAL) == -1) {
                                    perror("send() error");
                                }
                            }
                        }
                    }
                }
            }
        }

     }

    return 0;
}
