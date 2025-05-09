#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>

#define MAX_PEERS 10
#define MAX_FILES 10
#define MAX_FILENAME_LEN 100
#define MAX_BUF_SIZE 1024

// Structure representing a peer entry
struct peer_entry {
    uint32_t id;
    int socket_fd;
    int file_count;
    char files[MAX_FILES][MAX_FILENAME_LEN];
    struct sockaddr_in address;
};

struct peer_entry peers[MAX_PEERS];
int peer_count = 0;

// Finds the index of a peer based on its socket FD
int find_peer_by_socket(int socket_fd) {
    for (int i = 0; i < peer_count; i++) {
        if (peers[i].socket_fd == socket_fd)
            return i;
    }
    return -1;
}

// Finds a peer that has the requested file
int find_peer_with_file(const char *filename) {
    for (int i = 0; i < peer_count; i++) {
        for (int j = 0; j < peers[i].file_count; j++) {
            if (strcmp(peers[i].files[j], filename) == 0) {
                return i;
            }
        }
    }
    return -1;
}

// Removes a peer from the registry by socket FD
void remove_peer(int socket_fd) {
    int index = find_peer_by_socket(socket_fd);
    if (index != -1) {
        close(peers[index].socket_fd);
        peers[index] = peers[peer_count - 1];
        peer_count--;
    }
}

// Handles a JOIN request from a peer
void handle_join(int sockfd, uint32_t peer_id) {
    if (peer_count >= MAX_PEERS) return;

    int index = peer_count++;
    peers[index].id = peer_id;
    peers[index].socket_fd = sockfd;
    peers[index].file_count = 0;

    socklen_t addrlen = sizeof(peers[index].address);
    getpeername(sockfd, (struct sockaddr*)&peers[index].address, &addrlen);

    printf("TEST] JOIN %u\n", peer_id);
}

// Handles a PUBLISH request and stores filenames sent by the peer
void handle_publish(int sockfd, char *buf, int msg_len) {
    int index = find_peer_by_socket(sockfd);
    if (index == -1) return;

    int offset = 8;
    int count = 0;

    while (offset < msg_len && count < MAX_FILES) {
        int len = strnlen(buf + offset, MAX_FILENAME_LEN);
        if (len <= 0 || len >= MAX_FILENAME_LEN || offset + len + 1 > msg_len) break;
        strncpy(peers[index].files[count], buf + offset, MAX_FILENAME_LEN);
        count++;
        offset += len + 1;
    }

    peers[index].file_count = count;

    printf("TEST] PUBLISH %d", count);
    for (int i = 0; i < count; i++) {
        printf(" %s", peers[index].files[i]);
    }
    printf("\n");
}

// Handles a SEARCH request from a peer looking for a file
void handle_search(int sockfd, char *buf) {
    char *filename = buf + 8;
    int index = find_peer_with_file(filename);

    char response[14]; 
    memcpy(response, "SEARCHOK", 8);

    uint32_t id = 0;
    uint32_t ip = 0;
    uint16_t port = 0;

    if (index != -1) {
        ip = peers[index].address.sin_addr.s_addr;
        port = peers[index].address.sin_port;
        id = peers[index].id;

        memcpy(response + 8, &ip, 4);
        memcpy(response + 12, &port, 2);
    } else {
        memset(response + 8, 0, 6);
    }

    send(sockfd, response, 14, 0);

    struct in_addr addr;
    addr.s_addr = ip;
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr, ip_str, INET_ADDRSTRLEN);

    printf("TEST] SEARCH %s %u %s:%u\n",
        filename,
        id,
        index != -1 ? ip_str : "0.0.0.0",
        index != -1 ? ntohs(port) : 0
    );
}

// Main function initializes server and handles client communication
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int registry_fd, new_fd, max_fd;
    struct sockaddr_in registry_addr, peer_addr;
    socklen_t addrlen;
    char buffer[MAX_BUF_SIZE];

    // Create socket
    registry_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (registry_fd < 0) {
        perror("socket");
        exit(1);
    }

    // Allow port reuse
    int opt = 1;
    setsockopt(registry_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

     // Bind socket to given port
    memset(&registry_addr, 0, sizeof(registry_addr));
    registry_addr.sin_family = AF_INET;
    registry_addr.sin_port = htons(atoi(argv[1]));
    registry_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(registry_fd, (struct sockaddr *)&registry_addr, sizeof(registry_addr)) < 0) {
        perror("bind");
        close(registry_fd);
        exit(1);
    }

    // Listen for incoming connections
    if (listen(registry_fd, MAX_PEERS) < 0) {
        perror("listen");
        close(registry_fd);
        exit(1);
    }

    // Initialize FD sets for select()
    fd_set master_set, read_fds;
    FD_ZERO(&master_set);
    FD_SET(registry_fd, &master_set);
    max_fd = registry_fd;

    // Main server loop
    while (1) {
        read_fds = master_set;

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("select");
            exit(1);
        }

        // Check which sockets are ready
        for (int i = 0; i <= max_fd; i++) {
            if (!FD_ISSET(i, &read_fds)) continue;

            if (i == registry_fd) {
                addrlen = sizeof(peer_addr);
                new_fd = accept(registry_fd, (struct sockaddr*)&peer_addr, &addrlen);
                if (new_fd < 0) {
                    perror("accept");
                    continue;
                }
                FD_SET(new_fd, &master_set);
                if (new_fd > max_fd) max_fd = new_fd;
            } else {
                // Handle data from existing peer
                int bytes_received = recv(i, buffer, MAX_BUF_SIZE, 0);
                if (bytes_received <= 0) {
                    remove_peer(i);
                    FD_CLR(i, &master_set);
                    close(i);
                } else {
                    if (bytes_received < MAX_BUF_SIZE)
                        buffer[bytes_received] = '\0';
                    else
                        buffer[MAX_BUF_SIZE - 1] = '\0';

                     // Dispatch request based on command
                    if (strncmp(buffer, "JOIN", 4) == 0 && bytes_received >= 8) {
                        uint32_t peer_id;
                        memcpy(&peer_id, buffer + 4, 4);
                        handle_join(i, ntohl(peer_id));
                    } else if (strncmp(buffer, "PUBLISH", 7) == 0 && bytes_received >= 8) {
                        handle_publish(i, buffer, bytes_received);
                    } else if (strncmp(buffer, "SEARCH", 6) == 0 && bytes_received >= 8) {
                        handle_search(i, buffer);
                    }
                }
            }
        }
    }

    close(registry_fd);
    return 0;
}