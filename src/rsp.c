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
#include <unistd.h>

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

// Create a main func
int main(int argc, char *argv[]){
    printf("In main...\n");
    // Start off defining local vars
    char *server_port_string;
    char *backend_address;
    char *backend_port_string;

    struct addrinfo hints;
    struct addrinfo *address;
    struct addrinfo *address_iterator;

    int getaddrinfo_error;

    int server_socket_file_descriptor;
    int client_socket_file_descriptor;

    int so_reuseaddress;

    printf("Done with initialization of vars...\n");

    // Check if we have the right amount of cmd args adn assign them to vars
    if (argc != 4) {
        printf("Not enough arguments");

        fprintf(stderr,
                "Usage: %s  \n",
                argv[0]     // prints out "/mnt/d/Career/programming_Education/build_load_balancer_c/rsp"
                );
        exit(1);
    }

    printf("Caught correct number of args...\n");

    server_port_string = argv[1];
    backend_address = argv[2];
    backend_port_string = argv[3];

//    printf("Vars inspection:\n%s\n%s\n%s\n",
//           server_port_string,
//           backend_address,
//           backend_port_string);

    // Next step: get address of localhost in a similar fashion to the client connection.
    // But add 1 extra value to the hints & pass NULL as the first param
    memset(&hints, 0, sizeof(struct addrinfo));
//    memset(&hints, 0, sizeof hints);
    printf("Memory set...\n");      // Cannot use printf with only a string. Needs some form of an expression! Be aware.
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    printf("Hints defined...\n");

    getaddrinfo_error = getaddrinfo(NULL, server_port_string, &hints, &address);

    // AI_PASSIVE combined with NULL tells getaddrinfo we want to run a serv socket on this address
    // Able to listen for incoming conns, accept them and handle them
    // Once we have the list, we iterate through them, for each create a socket like before,
    // but instead of connecting, we bind so we can accept incoming conns
    for (address_iterator = address; address_iterator != NULL; address_iterator = address_iterator->ai_next){
        printf("Trying to define a file descriptor...\n");
        server_socket_file_descriptor = socket(address_iterator->ai_family,
                                               address_iterator->ai_socktype,
                                               address_iterator->ai_protocol);

        if(server_socket_file_descriptor == -1){
            continue;
        }
        printf("Caught a legit address!\n");
        so_reuseaddress = 1;
        setsockopt(server_socket_file_descriptor,
                   SOL_SOCKET,
                   SO_REUSEADDR,    // Share the socket to mitigate ctrl-c exits and subsequent resource alloc lag
                   &so_reuseaddress,
                   sizeof(so_reuseaddress));

        // Bind says "I own socket, and I'm listening for incoming conns"
        if(bind(server_socket_file_descriptor,
                address_iterator->ai_addr,
                address_iterator->ai_addrlen) == 0){
            break;
        }

        close(server_socket_file_descriptor);
    }

    printf("Bound to socket...\n");

    // Once we are bounded or are unable to bind, handle errs and tidy up
    if(address_iterator == NULL){
        fprintf(stderr, "Could not bind any addresses...\n");
        exit(1);
    }

    freeaddrinfo(address);
    printf("Freed address info...\n");

    // Finally, mark the socket as passive (listen instead of making outbound conns)
    listen(server_socket_file_descriptor, MAX_LISTEN_BACKLOG);
    printf("Listening...\n");

    // Now that we have a server sock ready, create an endless loop doing the proxying
    while(1){
        printf("Trying to define client socket file descriptor...\n");
        client_socket_file_descriptor = accept(server_socket_file_descriptor,
                                               NULL,
                                               NULL);
        printf("Client socket file descriptor defined...\n");

        if(client_socket_file_descriptor == -1){
            perror("Could not accept the socket...");
            exit(1);
        }

        // Now we have a client file descriptor that describes a client conn. We can hand this off to the
        // handle_client_connection func built earlier
        printf("All good. Handling client connection...\n");
        handle_client_connection(client_socket_file_descriptor,
                                 backend_address,
                                 backend_port_string);
    }
}