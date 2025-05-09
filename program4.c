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

#define MAX_FILES 10 // needs to be this large to store the file names from publish
#define MAX_FILE_SIZE 100 // sets the max file size based on given specifications

/*
 * Lookup a host IP address and connect to it using service. Arguments match the first two
 * arguments to getaddrinfo(3).
 *
 * Returns a connected socket descriptor or -1 on error. Caller is responsible for closing
 * the returned socket.
 */
int lookup_and_connect(const char *host, const char *service);

// *******************************************************************************
// EACH DECLARTION BELOW MUST BE DEFINED TO PROCESS DATA BASED ON PROGRAM 1 AND 2's
// IMPLEMENTATIONS OF THE PEER VERSIONS OF THEIR FUNCTIONS
struct peer_entry {
	uint32_t id;
	int socket_descriptor;
	char files[MAX_FILES][MAX_FILE_SIZE]; // the files published by the peer
	struct sockaddr_in address; 
	// contains the IP adddress and port number for the peer
};
/**
 * needs to support at least 5 peers
 * create an instance of the peer_entry struct to store the
 * current peer's values.
*/
void join(const int *s, char *buf, const uint32_t *peerID);
/**
 * peer must have joined
 * one publish per peer
 * max of 10 filenames
 * filename no longer than 100 characters including the null terminator
*/
void publish(const int *s, char *buf);
/**
 * all values must be in network byte order
 * if multiple peers have requested a file
 * you can respond with the file from any of those peers
*/
void search(const int *s, char *buf);
// *******************************************************************************

int main(int argc, char *argv[]) {
	char *host;
	char *server_port;
	uint32_t peerID;
	char buf[MAX_FILES];
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
		// processing of peer interaction
		
	}

}

void join(const int *s, char *buf, const uint32_t *peerID) {
	
}

void publish(const int *s, char *buf) {
    
}

void search(const int *s, char *buf) {
    
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
