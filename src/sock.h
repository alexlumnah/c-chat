#ifndef SOCK_H
#define SOCK_H

#include <stdint.h>
#include <stddef.h>
#include <sys/poll.h>

#define MAX_MESSAGE_LEN (65535)
#define MAX_CLIENTS     (255)

typedef enum {
    SOCK_SUCCESS = 0,
    SOCK_ERR_NO_DATA,
    SOCK_ERR_SOCKET_DISCONNECT,
    SOCK_ERR_NO_NEW_CONNECTIONS,
    SOCK_ERR_TOO_MANY_CONNECTIONS,
    SOCK_ERR_INVALID_MSG_LENGTH,
    SOCK_ERR_INVALID_MSG_FORMAT,
    SOCK_ERR_SEND_FAILURE,
    SOCK_ERR_POLL_FAILURE,
    SOCK_ERR_SERVER_START_FAILURE,
    SOCK_ERR_CLIENT_START_FAILURE,
    SOCK_ERR_UNINITIALIZED,
    SOCK_ERR_ALREADY_INITIALIZED,
    SOCK_ERR_CLIENT_NOT_FOUND,
    SOCK_ERR_INVALID_CMD,
    SOCK_ERR_CLIENT_STILL_ACTIVE,
} SocketStatus;

typedef enum {
    SOCK_UNINITIALIZED = 0, SOCK_SERVER, SOCK_CLIENT
} ConnectionType;

typedef struct Packet {
    uint16_t len;                       // Length of Packet in Bytes
    uint16_t sender;                    // Sender of message
    char data[MAX_MESSAGE_LEN];         // Actual message
    struct Packet* next_packet;         // Pointer to next message in queue
} Packet;

typedef enum ClientState {
    INACTIVE = 0,                       
    ACTIVE = 1,
} ClientState;

typedef struct Client {
    uint16_t id;                        // Unique Client id
    int fd;                             // Client Socket File Descriptor
    ClientState active;                 // Whether client is active or not
} Client;

typedef struct SocketState {

    ConnectionType type;                // Whether this is a server or client
    int socket;                         // Socket file descriptor

    uint16_t next_id;                   // Next unique client id
    int num_clients;                    // Current number of clients
    Client clients[MAX_CLIENTS];        // List of clients

    Packet* packet_queue;               // Incoming Packet Queue

} SocketState;


// General functions
SocketState* sock_get_state(void);                              // Get pointer to global state
SocketStatus poll_sockets(int timeout);                         // Poll sockets for incoming connections or messages

// Packet Queue Operations
int num_packets(void);                                          // Check how many messages are in the queue
Packet* pop_packet(void);                                       // Pop message at top of message queue and return pointer. Ownership passes to caller.

// Server Socket Functions
SocketStatus start_server_socket(char* port);                                   // Start a server on the local host at specified port
SocketStatus accept_client_socket(void);                                        // Accept any incoming connections, called from server poll
SocketStatus disconnect_client_socket(uint16_t client_id);                      // Close connection to a client
SocketStatus flush_inactive_client_sockets(void);                               // Stop tracking all inactive clients
SocketStatus server_socket_send_packet(uint16_t client_id, char* data, size_t num_bytes); // Send message from server to client
SocketStatus server_socket_recv_packet(uint16_t client_id);                     // Receive and unpack a message, store in message queue
SocketStatus shutdown_server_socket(void);                                      // Shutdown server

// Client Socket Functions
SocketStatus start_client_socket(char* host, char* port);                       // Start a client and connect to host at specified port
SocketStatus client_socket_send_packet(char* data, size_t num_bytes);           // Send message from client to server
SocketStatus shutdown_client_socket(void);                                      // Shutdown client

#endif // SOCK_H
