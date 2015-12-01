/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "3490"
#define BACKLOG 10     // how many pending connections queue will hold

void sigchld_handler(int s)
{
    int saved_errno = errno; // Save errors which may be overwritten here.

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

// Bind to first avaliable connection.
addrinfo* bind_connection(addrinfo *servinfo, int &sock){
    struct addrinfo *temp;
    int sock_set = 1;

    for(temp = servinfo; temp != NULL; temp = temp->ai_next) {
        if ((sock = socket(temp->ai_family, temp->ai_socktype, temp->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &sock_set, sizeof(int)) == -1) {
            perror("setsockopt");
            return NULL;
        }

        if (bind(sock, temp->ai_addr, temp->ai_addrlen) == -1) {
            close(sock);
            perror("server: bind");
            continue;
        }

        return temp;
    }


}

int main(int argc, char *argv[])
{
    int sockfd, new_fd;  // Listen on sock_fd, new connection on new_fd

    struct addrinfo connection; // Will define the connection type
    struct addrinfo *servinfo; // Contains structs for making connection - created by getaddrinfo()
    struct addrinfo *bound; // Will store the result of the bind operation.

    struct sockaddr_storage client_addr; // Connector address information
    socklen_t sin_size;

    if(argc != 2){
        fprintf(stderr, "usage: server message\n");
        return 1;
    }

    memset(&connection, 0, sizeof connection);
    connection.ai_family = AF_UNSPEC; // Using IPv6
    connection.ai_socktype = SOCK_STREAM; // Connect using TCP 
    connection.ai_flags = AI_PASSIVE; // Automatically detect IP of system running server

    int status;
    if ((status = getaddrinfo(NULL, PORT, &connection, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    bound = bind_connection(servinfo, sockfd);

    freeaddrinfo(servinfo); // Delete struct

    if (bound == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        return 1;
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        return 1;
    }

    struct sigaction act;
    memset(&act, 0, sizeof(act));

    act.sa_handler = sigchld_handler; // Points to a function used for deleting child processed to avoid zombie processes.

    sigemptyset(&act.sa_mask); // Makes sure no signals are blocked during execution of sigchld_handler.
    act.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &act, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    printf("server: waiting for connection...\n");

    char ip[INET_ADDRSTRLEN];

    while(1) {  // Accept connection
        sin_size = sizeof client_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(client_addr.ss_family, &((struct sockaddr_in*)&client_addr)->sin_addr, ip, sizeof ip);
        printf("server: got connection from %s\n", ip);

        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            if (send(new_fd, argv[1], strlen(argv[1]), 0) == -1)
                perror("send");
            close(new_fd);
            return 0;
        }
        close(new_fd);  // parent doesn't need this
    }

    return 0;
}