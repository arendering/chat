#include <sys/socket.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <strings.h>

#include <iostream>
#include <algorithm>
#include <list>

#define PORT "12345"
#define MAX_EVENTS 32
#define MAX_LEN 1024

int set_nonblock(int fd)
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
    set_nonblock(master_socket);
    listen(master_socket, SOMAXCONN);
    
    // epoll on read
    int epfd_read = epoll_create1(0);
    struct epoll_event event_read;
    event_read.data.fd = master_socket;
    event_read.events = EPOLLIN;
    epoll_ctl(epfd_read, EPOLL_CTL_ADD, master_socket, &event_read);

    // epoll on write
    int epfd_write = epoll_create1(0);
    struct epoll_event event_write;
    event_write.events = EPOLLOUT;

    while(true) {
       char buffer[MAX_LEN];
       bzero(buffer, MAX_LEN);

       struct epoll_event events_read[MAX_EVENTS];
       int ret_ev = epoll_wait(epfd_read, events_read, MAX_EVENTS, -1);
       for(int i = 0; i < ret_ev; ++i) {
           if(events_read[i].data.fd == master_socket) {
               struct sockaddr_in cli_addr;
               socklen_t cli_len = sizeof cli_addr;
               int slave_socket = accept(master_socket,
                                         (struct sockaddr *) &cli_addr,
                                         &cli_len);
               set_nonblock(slave_socket);
               struct epoll_event event_r;
               event_r.data.fd = slave_socket;
               event_r.events = EPOLLIN;
               epoll_ctl(epfd_read, EPOLL_CTL_ADD, slave_socket, &event_r);

               struct epoll_event event_w;
               event_w.data.fd = slave_socket;
               event_w.events = EPOLLOUT;
               epoll_ctl(epfd_write, EPOLL_CTL_ADD, slave_socket, &event_w);
           } else {
               int recv_res = recv(events_read[i].data.fd, buffer, MAX_LEN, 0);
               if(recv_res == 0 && errno != EAGAIN) {
                   shutdown(events_read[i].data.fd, SHUT_RDWR);
                   close(events_read[i].data.fd);
                   std::cout << "disconnected..\n";    
               } else {
                   struct epoll_event events_write[MAX_EVENTS];
                   int ret_ev = epoll_wait(epfd_write, events_write, MAX_EVENTS, -1);
                   for(int i = 0; i < ret_ev; ++i) {
                       send(events_write[i].data.fd, buffer, MAX_LEN, MSG_NOSIGNAL);
                   }
               }
           }
       }

    }

    return 0;
}
