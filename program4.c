/**
 * Group Members: Vincent Roberson and Muhammad I Sohail
 * ECEE 446 Section 1
 * Spring 2025
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <errno.h>

#define MAX_FILES 10
#define MAX_FILENAME_LEN 100
#define MAX_PEERS 100
#define BUF_SIZE 1024

struct peer_entry {
    uint32_t id;
    int socket_descriptor;
    char files[MAX_FILES][MAX_FILENAME_LEN];
    int file_count;
    struct sockaddr_in address;
};

struct peer_entry peers[MAX_PEERS];
int peer_count = 0;

void print_summary(const char *cmd, const char *args) {
    printf("TEST] %s %s\n", cmd, args);
    fflush(stdout);
}

// helper function to read exact n bytes
int read_n_bytes(int sock, char *buf, int n) {
    int total = 0;
    int bytes;
    while (total < n) {
        bytes = recv(sock, buf + total, n - total, 0);
        if (bytes <= 0) return bytes;
        total += bytes;
    }
    return total;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int listen_sock, new_sock, max_fd, activity, i;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    fd_set read_fds, master_fds;

    if ((listen_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(atoi(argv[1]));

    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(listen_sock, 5) < 0) {
        perror("listen");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }

    FD_ZERO(&master_fds);
    FD_SET(listen_sock, &master_fds);
    max_fd = listen_sock;

    while (1) {
        read_fds = master_fds;
        activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        if (activity < 0 && errno != EINTR) {
            perror("select");
            break;
        }

        for (i = 0; i <= max_fd; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == listen_sock) {
                    client_len = sizeof(client_addr);
                    new_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);
                    if (new_sock < 0) {
                        perror("accept");
                        continue;
                    }
                    FD_SET(new_sock, &master_fds);
                    if (new_sock > max_fd) max_fd = new_sock;
                } else {
                    char header[8];
                    int bytes = read_n_bytes(i, header, 4);
                    if (bytes <= 0) {
                        close(i);
                        FD_CLR(i, &master_fds);
                        for (int j = 0; j < peer_count; j++) {
                            if (peers[j].socket_descriptor == i) {
                                peers[j] = peers[--peer_count];
                                break;
                            }
                        }
                        continue;
                    }

                    uint32_t cmd;
                    memcpy(&cmd, header, 4);
                    cmd = ntohl(cmd);

                    if (cmd == 1) { // JOIN
                        bytes = read_n_bytes(i, header + 4, 4);
                        if (bytes <= 0) {
                            close(i);
                            FD_CLR(i, &master_fds);
                            continue;
                        }
                        uint32_t peer_id;
                        memcpy(&peer_id, header + 4, 4);
                        peer_id = ntohl(peer_id);

                        struct sockaddr_in addr;
                        socklen_t len = sizeof(addr);
                        getpeername(i, (struct sockaddr *)&addr, &len);

                        peers[peer_count].id = peer_id;
                        peers[peer_count].socket_descriptor = i;
                        peers[peer_count].address = addr;
                        peers[peer_count].file_count = 0;
                        peer_count++;

                        char msg[64];
                        snprintf(msg, sizeof(msg), "%u", peer_id);
                        print_summary("JOIN", msg);

                    } else if (cmd == 2) { // PUBLISH
                        bytes = read_n_bytes(i, header + 4, 4);
                        if (bytes <= 0) {
                            close(i);
                            FD_CLR(i, &master_fds);
                            continue;
                        }
                        uint32_t file_count;
                        memcpy(&file_count, header + 4, 4);
                        file_count = ntohl(file_count);

                        char filebuf[MAX_FILES * MAX_FILENAME_LEN];
                        int total_file_bytes = file_count * MAX_FILENAME_LEN;
                        if (read_n_bytes(i, filebuf, total_file_bytes) <= 0) {
                            close(i);
                            FD_CLR(i, &master_fds);
                            continue;
                        }

                        for (int j = 0; j < file_count; j++) {
                            strncpy(peers[peer_count - 1].files[j], filebuf + j * MAX_FILENAME_LEN, MAX_FILENAME_LEN);
                            peers[peer_count - 1].files[j][MAX_FILENAME_LEN - 1] = '\0';
                        }
                        peers[peer_count - 1].file_count = file_count;

                        char msg[512] = {0};
                        snprintf(msg, sizeof(msg), "%u", file_count);
                        for (int j = 0; j < file_count; j++) {
                            strncat(msg, " ", sizeof(msg) - strlen(msg) - 1);
                            strncat(msg, peers[peer_count - 1].files[j], sizeof(msg) - strlen(msg) - 1);
                        }
                        print_summary("PUBLISH", msg);

                    } else if (cmd == 3) { // SEARCH
                        char filename[MAX_FILENAME_LEN];
                        if (read_n_bytes(i, filename, MAX_FILENAME_LEN) <= 0) {
                            close(i);
                            FD_CLR(i, &master_fds);
                            continue;
                        }
                        filename[MAX_FILENAME_LEN - 1] = '\0';

                        bool found = false;
                        for (int j = 0; j < peer_count; j++) {
                            for (int k = 0; k < peers[j].file_count; k++) {
                                if (strcmp(filename, peers[j].files[k]) == 0) {
                                    char ip_str[INET_ADDRSTRLEN];
                                    inet_ntop(AF_INET, &peers[j].address.sin_addr, ip_str, sizeof(ip_str));
                                    char msg[128];
                                    snprintf(msg, sizeof(msg), "%s %u %s:%u",
                                             filename, peers[j].id, ip_str, ntohs(peers[j].address.sin_port));
                                    print_summary("SEARCH", msg);

                                    uint32_t id = htonl(peers[j].id);
                                    uint32_t ip = peers[j].address.sin_addr.s_addr;
                                    uint16_t port = peers[j].address.sin_port;
                                    send(i, &id, sizeof(uint32_t), 0);
                                    send(i, &ip, sizeof(uint32_t), 0);
                                    send(i, &port, sizeof(uint16_t), 0);

                                    found = true;
                                    break;
                                }
                            }
                            if (found) break;
                        }
                        if (!found) {
                            char msg[128];
                            snprintf(msg, sizeof(msg), "%s 0 0.0.0.0:0", filename);
                            print_summary("SEARCH", msg);

                            uint32_t zero = 0;
                            uint32_t zip = htonl(0);
                            uint16_t zport = 0;
                            send(i, &zero, sizeof(uint32_t), 0);
                            send(i, &zip, sizeof(uint32_t), 0);
                            send(i, &zport, sizeof(uint16_t), 0);
                        }
                    } else {
                        fprintf(stderr, "DEBUG: unknown command code %u\n", cmd);
                    }
                }
            }
        }
    }

    close(listen_sock);
    return 0;
}