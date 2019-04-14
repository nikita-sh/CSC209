#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#ifndef PORT
  #define PORT 30000
#endif
#define BUF_SIZE 128

int main(void) {
    char username[BUF_SIZE+1];
    printf("Enter username:\n");
    int usr_read = read(STDIN_FILENO, username, BUF_SIZE);
    if (usr_read == -1) {
        perror("client: read");
        exit(1);
    }
    username[usr_read - 1] = '\0';

    // Create the socket FD.
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("client: socket");
        exit(1);
    }

    // Set the IP and port of the server to connect to.
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server.sin_addr) < 1) {
        perror("client: inet_pton");
        close(sock_fd);
        exit(1);
    }

    // Connect to the server.
    if (connect(sock_fd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("client: connect");
        close(sock_fd);
        exit(1);
    }

    int usr_written = write(sock_fd, username, usr_read - 1);
    if (usr_written != usr_read - 1) {
        perror("client: write");
        exit(1);
    }

    // Read input from the user, send it to the server, and then accept the
    // echo that returns. Exit when stdin is closed.

    char buf[BUF_SIZE + 1];
    int max_fd = STDIN_FILENO >= sock_fd ? STDIN_FILENO : sock_fd;
    int num_read, num_written;
    fd_set all_fds;
    FD_ZERO(&all_fds);
    FD_SET(sock_fd, &all_fds);
    FD_SET(STDIN_FILENO, &all_fds);
    while (1) {
        fd_set listen_fds = all_fds;
        int nready = select(max_fd + 1, &listen_fds, NULL, NULL, NULL);
        if (nready == -1) {
            perror("client: select");
            exit(1);
        }

        if (FD_ISSET(sock_fd, &listen_fds)) {
            num_read = read(sock_fd, buf, BUF_SIZE);
            buf[num_read] = '\0';
            printf("Received from server: %s", buf);
        }

        if (FD_ISSET(STDIN_FILENO, &listen_fds)) {
            num_read = read(STDIN_FILENO, buf, BUF_SIZE);
            if (num_read == 0) {
                break;
            }
            buf[num_read] = '\0';

            num_written = write(sock_fd, buf, num_read);
            if (num_written != num_read) {
                perror("client: write");
                close(sock_fd);
                exit(1);
            }
        }
    }

    close(sock_fd);
    return 0;
}
