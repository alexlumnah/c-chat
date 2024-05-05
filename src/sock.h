#include <stdint.h>
#include <sys/poll.h>

#define MAX_MESSAGE_LEN (65535)
#define MAX_CLIENTS     (256)

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
} SocketError;

typedef enum {
    SOCK_UNINITIALIZED = 0, SOCK_SERVER, SOCK_CLIENT
} ConnectionType;

typedef struct Message {
    uint16_t len;                       // Length of Message in Bytes
    uint16_t sender;                    // Sender of message
    char data[MAX_MESSAGE_LEN];         // Actual message
    struct Message* next_msg;           // Pointer to next message in queue
} Message;

typedef enum ClientState {
    INACTIVE = 0,                       
    ACTIVE = 1,
} ClientState;

typedef struct Client {
    uint16_t id;                        // Unique Client id
    int fd;                             // Client Socket File Descriptor
    ClientState active;                 // Whether client is active or not
} Client;

typedef struct SockState {

    ConnectionType type;                // Whether this is a server or client
    int socket;                         // Socket file descriptor

    uint16_t next_id;                   // Next unique client id
    int num_clients;                    // Current number of clients
    Client clients[MAX_CLIENTS];        // List of clients

    Message* msg_queue;                 // Incoming Message Queue

} SockState;


// General functions
SockState* get_state(void);                                 // Get pointer to global state
int poll_sockets(int timeout);                              // Poll sockets for incoming connections or messages
int send_msg(int socket_fd, char* data, size_t num_bytes);  // Send data to a socket
int recv_msg(int socket_fd);                                // Receive and unpack a message, store in message queue
int id_to_fd(uint16_t id);                                  // Lookup id based on client fd
int fd_to_id(int fd);                                       // Lookup fd based on client id

// Message Queue Operations
int num_msgs(void);                                         // Check how many messages are in the queue
Message* pop_msg(void);                                     // Pop message at top of message queue and return pointer. Ownership passes to caller.

// Server Functions
int start_server(char* port);                               // Start a server on the local host at specified port
int accept_client(void);                                    // Accept any incoming connections, called from server poll
int disconnect_client(uint16_t client_id);                  // Close connection to a client
int flush_inactive_client(uint16_t id);                     // Stop tracking an inactive client
int server_send_msg(uint16_t client_id, char* data, size_t num_bytes); // Send message from server to client
int server_recv_msg(uint16_t client_id);                    // Receive and unpack a message, store in message queue
int shutdown_server(void);                                  // Shutdown server

// Client Functions
int start_client(char* host, char* port);                   // Start a client and connect to host at specified port
int client_send_msg(char* data, size_t num_bytes);          // Send message from client to server
int client_recv_msg(void);                                  // Receive and unpack a message, store in message queue
int shutdown_client(void);                                  // Shutdown client
