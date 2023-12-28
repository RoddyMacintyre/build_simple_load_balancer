//
// Created by might on 28/12/2023.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

// Constants for later use
#define MAX_LISTEN_BACKLOG 1
#define BUFFER_SIZE 4096

// Given connection to a client hitting the proxy:
// - connect to a backend
// - send client request to it
// - get response from backend
// - send back to client

void handle_client_connection(int client_socket_file_descriptor,    // low level way of describing an open file/socket/etc.
                              char *backend_host,
                              char *backend_port_string)
{
    // Some local variable definitions. Later C doesn't require this. Fix it later on.
    struct addrinfo hints;
    struct addrinfo *address;
    struct addrinfo *address_iterator;
    int getaddrinfo_error;

    int backend_socket_file_descriptor;
    char buffer[BUFFER_SIZE];
    int bytes_read;

    // Set everything in 'hints' to zero (local variable on the stack with undefined contents)
    memset(&hints, 0, sizeof(struct addrinfo));
    // Now fill it
    hints.ai_family = AF_UNSPEC;        // UNSPEC means either IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;    // Support socket streaming

    // Now that we have hints to work with, call getaddrinfo. Returns 0 on success, error code if not
    // Pass a pointer to a pointer to a struct.
    // We should get a pointer to the 1st item in a linkedlist of possible addresses that we can connect to in the
    // variable address
    getaddrinfo_error = getaddrinfo(backend_host, backend_port_string, &hints, &address);
    if (getaddrinfo_error != 0){
        fprintf(stderr, "Could not find the backend: %s\n", gai_strerror(getaddrinfo_error));
        exit(1);
    }

    // Find an address to connect to
    for(address_iterator = address; address_iterator != NULL; address_iterator = address_iterator->ai_next){
        // For each, try create a socket. On failure, move on.
        backend_socket_file_descriptor = socket(address_iterator->ai_family,
                                                address_iterator->ai_socktype,
                                                address_iterator->ai_protocol);
        if(backend_socket_file_descriptor == -1){
            continue;
        }
        // If success, try to connect, and if that succeeds, break the loop
        if(connect(backend_socket_file_descriptor,
                   address_iterator->ai_addr,
                   address_iterator->ai_addrlen) != -1){
            break;
        }

        // If connect failed, close the socket and move to next in the loop
        close(backend_socket_file_descriptor);
    }

    // Out of the loop, check if we did a successful socket create-connect. If not, exit
    if(address_iterator == NULL){
        fprintf(stderr, "Could not establish a connection to the backend");
        exit(1);
    }
    // Otherwise, freethe addresses list from getaddrinfo
    freeaddrinfo(address);

    // Now some code for the proxying. One read from the client, send everything to the backend
    bytes_read = read(client_socket_file_descriptor, buffer, BUFFER_SIZE);
    write(backend_socket_file_descriptor, buffer, bytes_read);
    // Loop through until 0 bytes
    while(bytes_read = read(backend_socket_file_descriptor, buffer, BUFFER_SIZE)){
        write(client_socket_file_descriptor, buffer, bytes_read);
    }
    // Close client socket
    close(client_socket_file_descriptor);
}