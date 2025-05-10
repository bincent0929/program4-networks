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

#define MAX_PEERS 5
#define MAX_FILES 10
#define MAX_FILENAME_LEN 100
#define MAX_BUF_SIZE 1024

// Structure representing a peer entry
struct peer_entry {
    uint32_t id;
    int socket_fd;
    int file_count;
    char files[MAX_FILES][MAX_FILENAME_LEN];
    struct sockaddr_storage address;
};

// peer_count and struct peers[MAX_PEERS]
int find_peer_by_socket(int socket_fd, int peer_count, struct peer_entry *peers);
int find_peer_with_file(const char *filename, int peer_count, struct peer_entry *peers);
void remove_peer(int socket_fd, int *peer_count, struct peer_entry *peers);
void handle_join(int sockfd, uint32_t peer_id, int *peer_count, struct peer_entry *peers, struct sockaddr *addr);
void handle_publish(int sockfd, char *buf, int msg_len, int peer_count, struct peer_entry *peers);
void handle_search(int sockfd, char *buf, int peer_count, struct peer_entry *peers);

// Main function initializes server and handles client communication
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

	struct peer_entry peers[MAX_PEERS];
	int peer_count = 0;
    
	// all_sockets stores all active sockets. Any socket connected to the server should
	// be included in the set. A socket that disconnects should be removed from the set.
	// The server's main socket should always remain in the set.
	fd_set all_sockets;
	FD_ZERO(&all_sockets);
	// call_set is a temporary used for each select call. Sockets will get removed from
	// the set by select to indicate each socket's availability.
	fd_set call_set;
	FD_ZERO(&call_set);

	// listen_socket is the fd on which the program can accept() new connections
	int listen_socket = bind_and_listen(htons(atoi(argv[1])));
	FD_SET(listen_socket, &all_sockets);

	// max_socket should always contain the socket fd with the largest value, just one
	// for now.
	int max_socket = listen_socket;

    // Main server loop
    while (1) {
        call_set = all_sockets;
		int num_s = select(max_socket+1, &call_set, NULL, NULL, NULL);
		if( num_s < 0 ){
			perror("ERROR in select() call");
			return -1;
		}
		// Check each potential socket.
		// Skip standard IN/OUT/ERROR -> start at 3.
		for( int s = 0; s <= max_socket; ++s ){
			// Skip sockets that aren't ready
			if( !FD_ISSET(s, &call_set) )
				continue;

			// A new connection is ready
			if( s == listen_socket ){
				// What should happen with a new connection?
				// You need to call at least one function here
				// and update some variables.
				struct sockaddr_storage remoteaddr;
				socklen_t addrlen = sizeof remoteaddr;
				int newsock = accept(listen_socket, (struct sockaddr*)&remoteaddr, &addrlen);
				FD_SET(newsock, &all_sockets);
				max_socket = find_max_fd(&all_sockets);
			}

			// A connected socket is ready
			else{
				// Put your code here for connected sockets.
				// Don't forget to handle a closed socket, which will
				// end up here as well.
				char buf[MAX_BUF_SIZE];
				int bytes_received = recv(s, buf, sizeof buf, 0);

                if (bytes_received <= 0) {
                    remove_peer(s, &peer_count, peers);
                    FD_CLR(s, &max_socket);
                    close(s);
                } else {
                    if (bytes_received < MAX_BUF_SIZE)
                        buf[bytes_received] = '\0';
                    else
                        buf[MAX_BUF_SIZE - 1] = '\0';

                     // Dispatch request based on command
                    if (strncmp(buf, "JOIN", 4) == 0 && bytes_received >= 8) {
                        uint32_t peer_id;
                        memcpy(&peer_id, buf + 4, 4);
                        handle_join(s, ntohl(peer_id), &peer_count, peers, &remoteaddr);
                    } else if (strncmp(buf, "PUBLISH", 7) == 0 && bytes_received >= 8) {
                        handle_publish(s, buf, bytes_received, peer_count, peers);
                    } else if (strncmp(buf, "SEARCH", 6) == 0 && bytes_received >= 8) {
                        handle_search(s, buf, peer_count, peers);
                    }
                }
			}
		}
    }
    close(listen_socket);
    return 0;
}

// Finds the index of a peer based on its socket FD
int find_peer_by_socket(int socket_fd, int peer_count, struct peer_entry *peers) {
    for (int i = 0; i < peer_count; i++) {
        if (peers[i].socket_fd == socket_fd)
            return i;
    }
    return -1;
}

// Finds a peer that has the requested file
int find_peer_with_file(const char *filename, int peer_count, struct peer_entry *peers) {
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
void remove_peer(int socket_fd, int *peer_count, struct peer_entry *peers) {
    int index = find_peer_by_socket(socket_fd, *peer_count, peers);
    if (index != -1) {
        close(peers[index].socket_fd);
        peers[index] = peers[*peer_count - 1]; // isn't this creating a repeat of the peer before it?
        (*peer_count)--;
        // it seems like all this does is overwrite the value at the index
        // then change the peer count so that we can't access the value in the final index
        // This copies the last peer into the slot of the peer being removed, then reduces the peer count.
    }
}

// Handles a JOIN request from a peer
void handle_join(int sockfd, uint32_t peer_id, int *peer_count, struct peer_entry *peers, struct sockaddr * remoteaddr) {
    if (*peer_count >= MAX_PEERS) return;

    int index = (*peer_count)++;
    peers[index].id = peer_id;
    peers[index].socket_fd = sockfd;
    peers[index].file_count = 0;
    peers[index].address = *peer_addr;

    socklen_t addrlen = sizeof(peers[index].address);
    getpeername(sockfd, (struct sockaddr*)&peers[index].address, &addrlen);

    printf("TEST] JOIN %u\n", peer_id);
}

// Handles a PUBLISH request and stores filenames sent by the peer
void handle_publish(int sockfd, char *buf, int msg_len, int peer_count, struct peer_entry *peers) {
    int index = find_peer_by_socket(sockfd, peer_count, peers);
    if (index == -1) return;

    int offset = 8;
    int count = 0;

    while (offset < msg_len && count < MAX_FILES) {
        int len = strnlen(buf + offset, MAX_FILENAME_LEN);
        if (len <= 0 || len >= MAX_FILENAME_LEN || offset + len + 1 > msg_len) break;
        strncpy(peers[index].files[count], buf + offset, MAX_FILENAME_LEN);
        count++; // why don't we iterate peers[index].file_count here? // count prevent partial or inconsistent updates
        // peers[index].file_count++
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
void handle_search(int sockfd, char *buf, int peer_count, struct peer_entry *peers) {
    char *filename = buf + 6;
    int index = find_peer_with_file(filename, peer_count, peers);

    char response[14];
    memcpy(response, "SEARCHOK", 8);

    uint32_t id = 0;
    uint32_t ip = 0;
    uint16_t port = 0;

    if (index != -1) {
        // could you indicate where these values are set for the peers? in the handle_join function
        struct sockaddr_in *addr_in = (struct sockaddr_in *)&peers[index].address;
        ip = addr_in->sin_addr.s_addr;
        port = addr_in->sin_port;
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

// ******************************************************************************
// For creating the server's connection
int find_max_fd(const fd_set *fs) {
	int ret = 0;
	for(int i = FD_SETSIZE-1; i>=0 && ret==0; --i){
		if( FD_ISSET(i, fs) ){
			ret = i;
		}
	}
	return ret;
}

int bind_and_listen( const char *service ) {
	struct addrinfo hints;
	struct addrinfo *rp, *result;
	int s;

	/* Build address data structure */
	memset( &hints, 0, sizeof( struct addrinfo ) );
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;

	/* Get local address info */
	if ( ( s = getaddrinfo( NULL, service, &hints, &result ) ) != 0 ) {
		fprintf( stderr, "stream-talk-server: getaddrinfo: %s\n", gai_strerror( s ) );
		return -1;
	}

	/* Iterate through the address list and try to perform passive open */
	for ( rp = result; rp != NULL; rp = rp->ai_next ) {
		if ( ( s = socket( rp->ai_family, rp->ai_socktype, rp->ai_protocol ) ) == -1 ) {
			continue;
		}

		if ( !bind( s, rp->ai_addr, rp->ai_addrlen ) ) {
			break;
		}

		close( s );
	}
	if ( rp == NULL ) {
		perror( "stream-talk-server: bind" );
		return -1;
	}
	if ( listen( s, MAX_PENDING ) == -1 ) {
		perror( "stream-talk-server: listen" );
		close( s );
		return -1;
	}
	freeaddrinfo( result );

	return s;
}
// ******************************************************************************