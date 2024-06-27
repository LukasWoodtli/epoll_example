#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>


/* UDP echo server */
int main(void) {

    int port = 5683;

    int udp_sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (udp_sock == -1) {
        perror("Could not create UDP socket");
        exit(EXIT_FAILURE);
    }

    int recvpktinfo_val = 1;
    int ret = setsockopt(udp_sock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &recvpktinfo_val, sizeof(recvpktinfo_val));
    if (ret == -1) {
        perror("Could not set socket option");
        exit(EXIT_FAILURE);
    }
    // TODO add more options

    // Set socket non-blocking
    int flags = fcntl(udp_sock, F_GETFD, 0);
    ret = fcntl(udp_sock, F_SETFL, flags | O_NONBLOCK);
    if (ret == -1) {
        perror("Could not set socket to non-blocking");
        exit(EXIT_FAILURE);
    }

    // Bind socket
    struct sockaddr_in6 server_addr;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_addr = in6addr_any;
    server_addr.sin6_port = htons(port);

    ret = bind(udp_sock, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (ret < 0) {
        perror("could not bind socket");
        exit(EXIT_FAILURE);
    }

    int epfd = epoll_create(1);
    if (epfd == -1) {
        perror("Could not create epoll fd");
        exit(EXIT_FAILURE);
    }

    struct epoll_event ev;
    ev.events = EPOLLIN; /* only input events (could use `EPOLLIN | EPOLLET` for edge triggered */
    ev.data.fd = udp_sock;
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, udp_sock, &ev);
    if (ret == -1) {
        perror("Can't register socket with epoll");
        exit(EXIT_FAILURE);
    }

    const size_t MAXEPOLLEVENTS = 3;
    struct epoll_event events[MAXEPOLLEVENTS];
    const int timeout_ms = 1000;

    while (1) {

        int ready = epoll_wait(epfd, events, MAXEPOLLEVENTS, timeout_ms);

        if (ready < 0) {
            perror("epoll_wait.");
            exit(EXIT_FAILURE);
        } else if (ready == 0) {
            /* timeout */
            continue;
        } else {
            for (int i = 0; i < ready; ++i) {
                if (events[i].data.fd == udp_sock) {
                    char recvbuf[1024];
                    memset(recvbuf, 0, sizeof(recvbuf));
                    int len;
                    struct sockaddr_in6 clientaddr;
                    socklen_t clilen = sizeof(clientaddr);

                    len = recvfrom(udp_sock, recvbuf, sizeof(recvbuf), 0, (struct sockaddr *) &clientaddr, &clilen);
                    if (len > 0) {
                        const size_t strlen = 64;
                        char host[strlen];
                        char service[strlen];
                        int res = getnameinfo((struct sockaddr *) &clientaddr,
                                              sizeof(clientaddr),
                                              host,
                                              sizeof(host),
                                              service,
                                              sizeof(service),
                                              0);
                        if (res) {
                            perror("getnameinfo");
                            exit(EXIT_FAILURE);
                        }
                        printf("client address = %s:%s\n", host, service);


                        printf("Recv: %s\n", recvbuf);
                        sendto(udp_sock, recvbuf, len, 0, (struct sockaddr *) &clientaddr, clilen);
                    }
                }
            }
        }
    }

    //close(epfd);
    //close(udp_sock);
    return 0;
}
