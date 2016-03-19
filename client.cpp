#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <stdio.h>

#include <iostream>

#define MAX_LEN 1024
#define SERV_ADDR "localhost"
#define SERV_PORT "3100"
#define UP "\x1b[1A"
#define ERASE_LINE "\x1b[2K"

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
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if(getaddrinfo(SERV_ADDR, SERV_PORT, &hints, &servinfo) != 0) {
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
    if(connect(master_socket, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        perror("connect() error");
        return 1;
    }
    freeaddrinfo(servinfo);
    SetNonblock(master_socket);
    SetNonblock(STDIN_FILENO);
    while(true) {
        fd_set set_on_read;
        FD_ZERO(&set_on_read);
        FD_SET(master_socket, &set_on_read);
        FD_SET(STDIN_FILENO, &set_on_read);

        int max = std::max(master_socket, STDIN_FILENO);

        if(select(max + 1, &set_on_read, NULL, NULL, NULL) == -1) {
            perror("select() error");
            break;
        }

        if(FD_ISSET(master_socket, &set_on_read)) {
            char buffer[MAX_LEN];
            int recv_res = recv(master_socket, buffer, MAX_LEN, 0);
            if(recv_res == 0 && errno != EAGAIN) {
                shutdown(master_socket, SHUT_RDWR);
                close(master_socket);
                break;
            } else {
                std::cout << buffer;
            }

        }
        if(FD_ISSET(STDIN_FILENO, &set_on_read)) {
            std::string input("");
            std::getline(std::cin, input);
            input.push_back('\n');
            std::cout << UP << ERASE_LINE;
            if(send(master_socket, input.c_str(),
                    input.length(), MSG_NOSIGNAL) == -1) {
                perror("send() error");
            }
        }
        
    }
    
    return 0;
}
