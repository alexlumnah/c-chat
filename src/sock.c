#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netdb.h>
#include <errno.h>
#include <sys/fcntl.h>

#include "sock.h"

#define PRINT_ERROR(msg) (fprintf(stderr, "[ERROR] %s Exit with error: %s\n", msg, strerror(errno)))
#define PRINT_ERROR2(msg1, msg2) (fprintf(stderr, "[ERROR] %s %s\n", msg1, msg2))
#define DEBUG_PRINT(msg) (printf("[DEBUG] %s\n", msg))
#define DEBUG_PRINT2(msg1, msg2) (printf("[DEBUG] %s %s\n", msg1, msg2))
#define DEBUG_PRINT3(msg1, val) (printf("[DEBUG] %s %d\n", msg1, val))

static ConnectionState connection;

ConnectionState* get_connection(void) {

    return &connection;
}

// Return number of messages in queue
int num_msgs(void) {
    
    int num_msgs = 0;

    Message* q_ptr = connection.msg_queue;
    while (q_ptr != NULL) {
        num_msgs++;
        q_ptr = q_ptr->next_msg;
    }

    return num_msgs;
}

// Pop message at top of message queue and return pointer. Ownership passes to caller.
Message* pop_msg(void) {

    Message* q_ptr = connection.msg_queue;
    if (q_ptr != NULL) {
        connection.msg_queue = connection.msg_queue->next_msg;
        q_ptr->next_msg = NULL;
    }

    return q_ptr;
}


// Send data to a socket
int send_msg(int socket_fd, char* data, size_t num_bytes) {

    int bytes_sent;
    uint16_t nw_len;
    char buffer[MAX_MESSAGE_LEN + 2] = {0};

    if (num_bytes > MAX_MESSAGE_LEN) return SOCK_ERR_INVALID_MSG_LENGTH;

    // Convert message to string of bytes in network order
    nw_len = htons((uint16_t)num_bytes + 1);    // Add one to end in 0
    memcpy(&buffer, &nw_len, 2);
    strncpy(&buffer[2], data, num_bytes);

    bytes_sent = send(socket_fd, &buffer, num_bytes + sizeof(nw_len), 0);

    if (bytes_sent == -1) return SOCK_ERR_SEND_FAILURE;

    return SOCK_SUCCESS;
}

// Receive and unpack a message, store in message queue
// Allocates memory for storage, hands ownership to queue owner
int recv_msg(int socket_fd) {

    ssize_t num_bytes;
    char buffer[MAX_MESSAGE_LEN] = {0};
    uint16_t msg_len = 0;

    // First check message length
    num_bytes = recv(socket_fd, &msg_len, sizeof(msg_len), 0);

    if (num_bytes == -1) {
        DEBUG_PRINT("No message read.");
        return SOCK_ERR_NO_DATA;
    } else if (num_bytes == 0) {
        DEBUG_PRINT("Socket disconnected.");
        return SOCK_ERR_SOCKET_DISCONNECT;
    } else if (num_bytes != 2) {
        DEBUG_PRINT("Invalid message format.");
        return SOCK_ERR_INVALID_MSG_FORMAT;
    }

    // Convert to host endianness
    msg_len = ntohs(msg_len);

    DEBUG_PRINT3("Message length:",msg_len);

    if (msg_len > MAX_MESSAGE_LEN) {
        DEBUG_PRINT("Invalid message length.");
        return SOCK_ERR_INVALID_MSG_LENGTH;
    }

    // Attempt to receive rest of message
    num_bytes = recv(socket_fd, &buffer, msg_len, 0);

    // TODO - ADD CAPABILITY FOR NOT GETTING ENTIRE MESSAGE
    if (num_bytes == 0) {
        return SOCK_ERR_SOCKET_DISCONNECT;
    } else if (num_bytes != msg_len) {
        DEBUG_PRINT("Did not receive entire message.");
        return SOCK_ERR_INVALID_MSG_LENGTH;
    }

    DEBUG_PRINT2("Message:", buffer);

    // Now construct message
    Message *msg = calloc(1, sizeof(Message));
    msg->len = msg_len;
    strncpy(msg->data, buffer, msg_len);

    // Check who the message is received from
    // This is inefficient, but allows for a simpler interface
    // Default sender id is 0 for clients
    for (int i = 0; i < connection.num_clients; i++) {
        if (connection.clients[i].socket == socket_fd) {
            msg->sender = connection.clients[i].id;
            break;
        }
    }

    // Find end of message queue, and add message to end
    Message* queue_end = connection.msg_queue;
    if (connection.msg_queue == NULL) {
        connection.msg_queue = msg;
    } else {
        while (queue_end->next_msg != NULL) {
            queue_end = queue_end->next_msg;
        }
        queue_end->next_msg = msg;
    }

    return SOCK_SUCCESS;
}

// Start a server on the local host at specified port
int start_server(char* port) {

    memset(&connection, 0, sizeof connection);   // Clear out state

    int status;             // Variable for storing function return status
    int socket_fd;           // Variable for storing socket file descriptor

    struct addrinfo hints = {0}; // Struct to pass inputs to getaddrinfo
    struct addrinfo *addr;       // Struct to get results from getaddrinfo

    // If already initialized, return error
    if (connection.type != SOCK_UNINITIALIZED) return SOCK_ERR_ALREADY_INITIALIZED;

    hints.ai_family = AF_UNSPEC;        // Either IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;    // TCP Stream Socket
    hints.ai_flags = AI_PASSIVE;        // Fill in IP

    status = getaddrinfo(NULL, port, &hints, &addr);
    if (status != 0) {
        PRINT_ERROR2("Unable to get address.", gai_strerror(status));
        return SOCK_ERR_SERVER_START_FAILURE;
    }

    // Open a streaming socket
    socket_fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (socket_fd == -1) {
        PRINT_ERROR("Unable to open socket.");
        return SOCK_ERR_SERVER_START_FAILURE;
    }

    // Make this socket nonblocking
    status = fcntl(socket_fd, F_SETFL, O_NONBLOCK);
    if (status != 0) {
        PRINT_ERROR("Unable to configure socket to non-blocking.");
        return SOCK_ERR_SERVER_START_FAILURE;
    }

    // Allow other sockets to bind to this port if no one is listening
    int opt = 1;
    status = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (status != 0) {
        PRINT_ERROR("Unable to allow other sockets to bind to port.");
        return SOCK_ERR_SERVER_START_FAILURE;
    }

    // Bind socket to our ip and port
    status = bind(socket_fd, addr->ai_addr, addr->ai_addrlen);
    if (status != 0) {
        PRINT_ERROR("Unable to bind to port.");
        return SOCK_ERR_SERVER_START_FAILURE;
    }

    // Begin listening for connections on socket, with maxium backlog of 10
    status = listen(socket_fd, 10);
    if (status != 0) {
        PRINT_ERROR("Unable to listen on socket.");
        return SOCK_ERR_SERVER_START_FAILURE;
    }

    // Update type, and save socked fd
    connection.type = SOCK_SERVER;
    connection.socket = socket_fd;

    // Default id of server is 0
    connection.id = 0;
    connection.next_id++;


    // Add our socket to the list of sockets to poll for inputs
    connection.fds[0].fd = connection.socket;
    connection.fds[0].events = POLLIN;
    connection.fds[0].revents = 0;

    // Setup our message queue
    connection.msg_queue = NULL;

    return SOCK_SUCCESS;
}

// Accept any incoming connections, add to client list
int accept_client(void) {
    
    int client_socket;
    struct sockaddr_storage cli_addr;
    socklen_t addr_len = sizeof cli_addr;

    // Accept incoming connections
    client_socket = accept(connection.socket, (struct sockaddr *)&cli_addr, &addr_len);
    
    if (client_socket == -1) return SOCK_ERR_UNABLE_TO_ACCEPT;

    if (connection.num_clients >= MAX_CLIENTS) {
        close(client_socket);
        return SOCK_ERR_TOO_MANY_CONNECTIONS;
    }

    // On success, Add to client list
    connection.clients[connection.num_clients].id = connection.next_id;
    connection.clients[connection.num_clients].socket = client_socket;

    // Add to file descriptor polling list
    connection.fds[connection.num_clients + 1].fd = client_socket;
    connection.fds[connection.num_clients + 1].events = POLLIN;
    connection.fds[connection.num_clients + 1].revents = 0;

    connection.next_id++;
    connection.num_clients++;

    printf("[New client connection. id: %d Socket: %d]\n", connection.clients[connection.num_clients - 1].id, client_socket);

    return SOCK_SUCCESS;
}

// Close connection to client
int disconnect_client(Client c) {

    // Remove from client and fd list by overwriting entry with final entry
    for (int i = 0; i < connection.num_clients; i++) {
        if (c.id == connection.clients[i].id) {
            printf("[Disconnecting client id: %d Socket: %d]\n", c.id, c.socket);

            connection.clients[i] = connection.clients[connection.num_clients - 1];
            connection.fds[i + 1] = connection.fds[connection.num_clients];

            connection.num_clients--;

            // Overwrite data
            memset(&connection.clients[connection.num_clients], 0, sizeof(struct Client));
            memset(&connection.fds[connection.num_clients + 1], 0, sizeof(struct pollfd));

            close(c.socket);

            return SOCK_SUCCESS;
        }
    }

    return SOCK_ERR_CLIENT_NOT_FOUND;
}

// Send data to client
int server_send_msg(int16_t id, char* data, size_t num_bytes) {

    // TODO - Figure out a cleaner solution here, do we remove client ids all together?
    int socket = 0;
    for (int i = 0; i < connection.num_clients; i++) {
        if (connection.clients[i].id == id) {
            socket = connection.clients[i].socket;
            break;
        }
    }

    return send_msg(socket, data, num_bytes);
}

// Shutdown server and all client connections
int shutdown_server(void) {

    printf("Shutting down connection.\n");
    for (int i = 0; i < connection.num_clients; i++) {
        disconnect_client(connection.clients[i]);
    }

    close(connection.socket);

    memset(&connection, 0, sizeof(ConnectionState));

    return SOCK_SUCCESS;
}

// Poll connection for connections or messages
// Accept any new connections, and add new messages to queue
int poll_sockets(int timeout) {

    // Poll for any activity
    int num_events;
    int status;

    num_events = poll(connection.fds, connection.num_clients + 1, timeout);

    if (num_events < 0) return SOCK_ERR_POLL_FAILURE;

    if (connection.type == SOCK_SERVER) {
        // First check our connection for any incoming requests
        if (connection.fds[0].revents & POLLIN) {
            DEBUG_PRINT("Polled new connection");
            accept_client();
        }

        // Now check remaining ports for messages
        // Store any disconnected clients in list to disconnect later
        int disconnected_ids[MAX_CLIENTS] = {0};
        int num_disconnected = 0;
        for (int i = 1; i < connection.num_clients + 1; i++) {
            if (connection.fds[i].revents & POLLIN) {
                DEBUG_PRINT("Polled new message");
                status = recv_msg(connection.fds[i].fd);
                if (status == SOCK_ERR_SOCKET_DISCONNECT) {
                    disconnected_ids[num_disconnected] = connection.clients[i - 1].id;
                    num_disconnected++;
                }
            }
        }

        // Now disconnect all clients that were disconnected, starting at the back
        for (int i = 0; i < num_disconnected; i++) {
            for (int j = connection.num_clients - 1; j >= 0; j--) {
                if (connection.clients[j].id == disconnected_ids[i]) {
                    disconnect_client(connection.clients[j]);
                    break;
                }
            }
        }
    } else if (connection.type == SOCK_CLIENT) {
        // Check if our client socket has any messages
        if (connection.fds[0].revents & POLLIN) {
            DEBUG_PRINT("Polled new message");
            status = recv_msg(connection.fds[0].fd);
            if (status == SOCK_ERR_SOCKET_DISCONNECT) {
                DEBUG_PRINT("Server disconnected.");
                shutdown_client();
                return SOCK_ERR_SOCKET_DISCONNECT;
            }
        }
    }


    return SOCK_SUCCESS;
}

// Start a client and connect to host at specified port
int start_client(char* host, char* port) {

    int status;             // Variable for storing function return status
    int socket_fd;           // Variable for storing socket file descriptor

    struct addrinfo hints = {0}; // Struct to pass inputs to getaddrinfo
    struct addrinfo *addr;       // Struct to get results from getaddrinfo

    // If already initialized, return error
    if (connection.type != SOCK_UNINITIALIZED) return SOCK_ERR_ALREADY_INITIALIZED;

    hints.ai_family = AF_UNSPEC;        // Either IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;    // TCP Stream Socket
    hints.ai_flags = AI_PASSIVE;        // Fill in IP

    status = getaddrinfo(host, port, &hints, &addr);
    if (status != 0) {
        fprintf(stderr, "Unable to get address: %s\n", gai_strerror(status));
        return SOCK_ERR_CLIENT_START_FAILURE;
    }

    // Open a streaming socket
    socket_fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (socket_fd == -1) {
        PRINT_ERROR("Unable to open socket.");
        return SOCK_ERR_CLIENT_START_FAILURE;
    }

    // Connect to our socket
    status = connect(socket_fd, addr->ai_addr, addr->ai_addrlen);
    if (status == -1) {
        PRINT_ERROR("Unable to connect to socket.");
        return SOCK_ERR_CLIENT_START_FAILURE;
    }

    // Make this socket nonblocking
    status = fcntl(socket_fd, F_SETFL, O_NONBLOCK);
    if (status != 0) {
        PRINT_ERROR("Unable to configure socket to non-blocking.");
        return SOCK_ERR_CLIENT_START_FAILURE;
    }

    // Update type, and save socked fd
    connection.type = SOCK_CLIENT;
    connection.socket = socket_fd;

     // Add our socket to the list of sockets to poll for inputs
    connection.fds[0].fd = connection.socket;
    connection.fds[0].events = POLLIN;
    connection.fds[0].revents = 0;

    // Setup our message queue
    connection.msg_queue = NULL;

    return SOCK_SUCCESS;
}

// Send message from client to server
int client_send_msg(char* data, size_t num_bytes) {

    return send_msg(connection.socket, data, num_bytes);
}

// Shutdown client
int shutdown_client(void) {

    printf("Shutting down client.\n");

    close(connection.socket);

    memset(&connection, 0, sizeof(ConnectionState));

    return SOCK_SUCCESS;
}
