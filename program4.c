/**
 * Group Members: Vincent Roberson and Muhammad I Sohail
 * ECEE 446 Section 1
 * Spring 2025
 */
#include <stdio.h> // for file io
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <dirent.h> // for reading file names in a direcory for the publish function
#include <arpa/inet.h>

#define MAX_SIZE 1200 // needs to be this large to store the file names from publish
#define MAX_FILE_SIZE 100 // sets the max file size based on given specifications

/*
 * Lookup a host IP address and connect to it using service. Arguments match the first two
 * arguments to getaddrinfo(3).
 *
 * Returns a connected socket descriptor or -1 on error. Caller is responsible for closing
 * the returned socket.
 */
int lookup_and_connect(const char *host, const char *service);
/**
 * This is just the example struct
 */
struct peer_entry {
	uint32_t id;
	int socket_descriptor;
	char files[MAX_SIZE][MAX_FILE_SIZE];
	struct sockaddr_in address;
};
/**
 * sends the join request to the registry to join the network
 * only needs to happen once per program run
 * sends a 1 byte field of 0, then a 4 byte peer ID
 * The peer ID must be in network byte order
 * each peer ID must be unique and 
 * is provided by a command line argument
*/
void join(const int *s, char *buf, const uint32_t *peerID);
/**
 * Informs the registry of what files are available to share
 * opens, read, then counts the files in the "SharedFiles" directory
 * any files in the directory are then added to the registry index
 * 1 byte for action = 1, 4 bytes for the file count, variable bytes of null terminated for the file names
 * must contain Count file names in total with exactly NULL characters
 * Count must be in network byte order
 * each filename is at most 100 bytes (including NULL)
 * no unused bytes between filenames
 * a publish cannot be larger than 1200 bytes (12 files)
*/
void publish(const int *s, char *buf);
/**
 * look for peers with a desired filename
 * a request with the name of the file is sent from the peer
 * the registry sends a search response after it receives a search request
 * the response indicates that another peer has the file requested
 * if the peer is looking for a file published by the peer, it won't locate it
 * a search request has 1 byte containing 2, then variable bytes for the desired null-terminated filename
 * a search response (sent from the registry to the requesting peer) contains
 * 4 bytes for a peer ID, 4 bytes for an IPv4 address, and 2 bytes for a peer port
 * if the file is not found, then the response will contain zeros (or if the peer themself has the file)
 * user inputs the name of the file on a newline after SEARCH is entered
*/
void search(const int *s, char *buf);
/**
 * Fetch a file from another peer and save it locally
 * 1. read file name from terminal
 * 2. send a search request to the registry for the file
 * 3. receive peer information from the registry
 * 4. send a FETCH to the identified peer
 * 5. Recieve and save the file from the peer and save it 
 * to a file with the same name as the user requested in the local
 * directory of the peer application.
 */
void fetch(const int *s, char *buf);

int main(int argc, char *argv[]) {
	char *host;
	char *server_port;
	uint32_t peerID;
	char buf[MAX_SIZE];
	int s;
    char userChoice[10];
	bool hasJoined = false;

	if ( argc == 4 ) {
		host = argv[1];
		server_port = argv[2];
		peerID = atoi(argv[3]);
	}
	else {
		fprintf( stderr, "usage: %s host\n", argv[0] );
		exit( 1 );
	}

	/* Lookup IP and connect to server */
	if ( ( s = lookup_and_connect( host, server_port ) ) < 0 ) {
		exit( 1 );
	}

	while(1) {
		printf("What would you like to do?: \n");
		scanf("%s", userChoice);
		if(strcmp(userChoice, "JOIN") == 0) {
			if (hasJoined) {
				printf("Error: Already joined the registry.\n");
			} else {
				printf("Joining the registry with Peer ID: %u\n", peerID);
			join(&s, buf, &peerID);
			hasJoined = true;
			}
			continue;
		}
		else if (strcmp(userChoice, "PUBLISH") == 0) {
			if (hasJoined == true) {
				publish(&s, buf);
				continue;
			}
			else {
				printf("You must join before you can publish \n");
				continue;
			}
		}
		else if (strcmp(userChoice, "SEARCH") == 0) {
			if (hasJoined == true) {
				search(&s, buf);
				continue;
			}
			else {
				printf("You must join before you can search \n");
				continue;
			}
		}
		else if (strcmp(userChoice, "FETCH") == 0) {
			if (hasJoined == true) {
				fetch(&s, buf);
				continue;
			} else {
				printf("You must join before you can fetch\n");
				continue;
			}
		}		
		else if (strcmp(userChoice, "EXIT") == 0) {
			close( s );
			// closes the socket
			return 0;
			// ends program
		}
	}

}

void join(const int *s, char *buf, const uint32_t *peerID) {
	buf[0] = 0;
	// apparently a byte of 0 is the null character
	uint32_t net_peerID = htonl(*peerID);
	memcpy(buf + 1, &net_peerID, sizeof(uint32_t));
	/*
		this should save the peerID given by the user
		from buf[1] to buf[4]
	*/
	send(*s, buf, 5, 0);
}

void publish(const int *s, char *buf) {
    buf[0] = 1;
	uint32_t count = 0;
	int fileNameOffset = 5;

	// Where I got this:
	// https://chatgpt.com/share/67c8abd2-5d50-800a-853f-55de0a46d0c1
	DIR *d;
	struct dirent *dir;
	d = opendir("SharedFiles");
	while ((dir = readdir(d)) != NULL) {
		if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
        	continue;  // Skip these special directory entries
    	}
		else {
			count++;
			// Where I go this
			// https://chatgpt.com/share/67c8abd2-5d50-800a-853f-55de0a46d0c1
			strcpy(&buf[fileNameOffset], dir->d_name);
			// dir->d_name is the current filename
			fileNameOffset += strlen(dir->d_name) + 1;
		}
	}
	closedir(d);

	uint32_t net_count = htonl(count);
	memcpy(buf + 1, &net_count, sizeof(uint32_t));
    int send_size = fileNameOffset;
    printf("Sending PUBLISH request: Action=%d, Count=%d, Total Size=%d\n", buf[0], count, send_size);
    if (send(*s, buf, send_size, 0) == -1) {
        perror("Error sending PUBLISH request");
    }
}

void search(const int *s, char *buf) {
    char filename[MAX_FILE_SIZE];
	// filename is assumed to be max 100 bytes by the documentation
    printf("Enter a file name: ");
    scanf("%s", filename);

    // Construct the search request
    buf[0] = 2; // Action field for SEARCH request
    strcpy(buf + 1, filename); // Copy filename into buffer, ensuring null termination

    // Send the search request
    int msg_len = 1 + strlen(filename) + 1; // 1 byte action + filename + NULL terminator
    send(*s, buf, msg_len, 0);

    // Receive the response
    int received = recv(*s, buf, 10, 0); // Expecting a 10-byte response
    if (received < 0) {
        perror("recv");
        return;
    }

    // Parse the response
    uint32_t peer_id, peer_ip;
    uint16_t peer_port;
    memcpy(&peer_id, buf, 4);
    memcpy(&peer_ip, buf + 4, 4);
    memcpy(&peer_port, buf + 8, 2);

    peer_id = ntohl(peer_id);
    peer_port = ntohs(peer_port);

    // Check if file was found
    if (peer_id == 0 && peer_ip == 0 && peer_port == 0) {
        printf("File not indexed by registry\n");
    } else {
        struct in_addr ip_addr;
        ip_addr.s_addr = peer_ip;
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip_addr, ip_str, INET_ADDRSTRLEN);

        printf("File found at\n");
        printf("Peer %u\n", peer_id);
        printf("%s:%u\n", ip_str, peer_port);
    }
}

void fetch(const int *s, char *buf) {
	char filename[MAX_FILE_SIZE];
	printf("Enter a file name: ");
	scanf("%s", filename);

	// Send search request
	buf[0] = 2;
	strcpy(buf + 1, filename);
	int msg_len = 1 + strlen(filename) + 1;
	send(*s, buf, msg_len, 0);

	// Receive registry response (10 bytes: 4 ID + 4 IP + 2 Port)
	int received = recv(*s, buf, 10, 0);
	if (received < 0) {
		perror("recv");
		return;
	}

	uint32_t peer_id, peer_ip;
	uint16_t peer_port;
	memcpy(&peer_id, buf, 4);
	memcpy(&peer_ip, buf + 4, 4);
	memcpy(&peer_port, buf + 8, 2);
	peer_id = ntohl(peer_id);
	peer_port = ntohs(peer_port);

	if (peer_id == 0 && peer_ip == 0 && peer_port == 0) {
		printf("File not indexed by registry\n");
		return;
	}

	// Convert IP to string
	struct in_addr ip_addr;
	ip_addr.s_addr = peer_ip;
	char ip_str[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &ip_addr, ip_str, INET_ADDRSTRLEN);
	printf("Fetching file from Peer %u at %s:%u\n", peer_id, ip_str, peer_port);

	// Connect to peer
	char port_str[10];
	sprintf(port_str, "%u", peer_port);
	int peer_sock = lookup_and_connect(ip_str, port_str);
	if (peer_sock < 0) {
		fprintf(stderr, "Failed to connect to peer\n");
		return;
	}

	// Send FETCH request
	char fetch_req[MAX_FILE_SIZE + 1];
	fetch_req[0] = 3;
	strcpy(fetch_req + 1, filename);
	int fetch_len = 1 + strlen(filename) + 1;
	send(peer_sock, fetch_req, fetch_len, 0);

	// Receive FETCH response (1 byte response code)
	char response_code;
	int res = recv(peer_sock, &response_code, 1, 0);
	if (res != 1) {
		perror("Error receiving response code");
		close(peer_sock);
		return;
	}

	// Open file for writing
	if (response_code != 0) {
		printf("Peer unable to send file.\n");
		close(peer_sock);
		return;
	}

	FILE *fp = fopen(filename, "wb");
	if (!fp) {
		perror("Error opening file to write");
		close(peer_sock);
		return;
	}

	// Receive file data
	char file_buf[1024];
	int bytes;
	while ((bytes = recv(peer_sock, file_buf, sizeof(file_buf), 0)) > 0) {
		fwrite(file_buf, 1, bytes, fp);
	}

	fclose(fp);
	close(peer_sock);
	printf("File %s downloaded successfully.\n", filename);
}

int lookup_and_connect( const char *host, const char *service ) {
	struct addrinfo hints;
	struct addrinfo *rp, *result;
	int s;

	/* Translate host name into peer's IP address */
	memset( &hints, 0, sizeof( hints ) );
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	if ( ( s = getaddrinfo( host, service, &hints, &result ) ) != 0 ) {
		fprintf( stderr, "stream-talk-client: getaddrinfo: %s\n", gai_strerror( s ) );
		return -1;
	}

	/* Iterate through the address list and try to connect */
	for ( rp = result; rp != NULL; rp = rp->ai_next ) {
		if ( ( s = socket( rp->ai_family, rp->ai_socktype, rp->ai_protocol ) ) == -1 ) {
			continue;
		}

		if ( connect( s, rp->ai_addr, rp->ai_addrlen ) != -1 ) {
			break;
		}

		close( s );
	}
	if ( rp == NULL ) {
		perror( "stream-talk-client: connect" );
		return -1;
	}
	freeaddrinfo( result );

	return s;
}
