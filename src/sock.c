#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <sys/fcntl.h>

#include "sock.h"

#define PRINT_ERROR(msg) (fprintf(stderr, "[ERROR] %s Exit with error: %s\n", msg, strerror(errno)))
#define PRINT_ERROR2(msg1, msg2) (fprintf(stderr, "[ERROR] %s %s\n", msg1, msg2))

#ifdef DEBUG
#define DEBUG_PRINT(msg) (printf("[DEBUG] %s\n", msg))
#define DEBUG_PRINT2(msg1, msg2) (printf("[DEBUG] %s %s\n", msg1, msg2))
#define DEBUG_PRINT3(msg, val) (printf("[DEBUG] %s %d\n", msg, val))
#else
#define DEBUG_PRINT(msg)
#define DEBUG_PRINT2(msg1, msg2)
#define DEBUG_PRINT3(msg, val)
#endif

static SocketState connection;

// Lookup id based on client fd
static int id_to_fd(uint16_t id) {

    for (int i = 0; i < connection.num_clients; i++) {
        if (connection.clients[i].id == id) {
            return connection.clients[i].fd;
        }
    }

    return -1;  
}

// Lookup fd based on client id                                 
static int fd_to_id(int fd) {

    for (int i = 0; i < connection.num_clients; i++) {
        if (connection.clients[i].fd == fd) {
            return connection.clients[i].id;
        }
    }

    return -1;  
}

// Send data to a socket
static SocketStatus send_packet(int socket_fd, const char* data, size_t num_bytes) {

    int bytes_sent;
    uint16_t nw_len;
    char buffer[MAX_MESSAGE_LEN + 2] = {0};

    if (connection.type == SOCK_UNINITIALIZED) return SOCK_ERR_UNINITIALIZED;
    if (num_bytes > MAX_MESSAGE_LEN) return SOCK_ERR_INVALID_MSG_LENGTH;

    // Convert packet to string of bytes in network order
    nw_len = htons((uint16_t)num_bytes);
    memcpy(&buffer, &nw_len, 2);
    memcpy(&buffer[2], data, num_bytes);

    bytes_sent = send(socket_fd, &buffer, num_bytes + sizeof(nw_len), 0);

    if (bytes_sent == -1) return SOCK_ERR_SEND_FAILURE;

    return SOCK_SUCCESS;
}

// Receive and unpack a packet, store in packet queue
// Allocates memory for storage, hands ownership to queue owner
static SocketStatus recv_packet(int socket_fd) {

    ssize_t num_bytes;
    char buffer[MAX_MESSAGE_LEN] = {0};
    uint16_t packet_len = 0;

    if (connection.type == SOCK_UNINITIALIZED) return SOCK_ERR_UNINITIALIZED;

    // First check packet length
    num_bytes = recv(socket_fd, &packet_len, sizeof(packet_len), 0);

    if (num_bytes == -1) {
        DEBUG_PRINT("No packet read.");
        return SOCK_ERR_NO_DATA;
    } else if (num_bytes == 0) {
        DEBUG_PRINT("Socket disconnected.");
        return SOCK_ERR_SOCKET_DISCONNECT;
    } else if (num_bytes != 2) {
        DEBUG_PRINT("Invalid packet format.");
        return SOCK_ERR_INVALID_MSG_FORMAT;
    }

    // Convert to host endianness
    packet_len = ntohs(packet_len);

    DEBUG_PRINT3("Packet length:",packet_len);

    if (packet_len > MAX_MESSAGE_LEN) {
        DEBUG_PRINT("Invalid packet length.");
        return SOCK_ERR_INVALID_MSG_LENGTH;
    }

    // Attempt to receive rest of packet
    num_bytes = recv(socket_fd, &buffer, packet_len, 0);

    // TODO - ADD CAPABILITY FOR NOT GETTING ENTIRE MESSAGE
    if (num_bytes == 0) {
        return SOCK_ERR_SOCKET_DISCONNECT;
    } else if (num_bytes != packet_len) {
        DEBUG_PRINT("Did not receive entire packet.");
        return SOCK_ERR_INVALID_MSG_LENGTH;
    }

    DEBUG_PRINT2("Packet:", buffer);

    // Now construct packet
    Packet *packet = calloc(1, sizeof(Packet));
    packet->len = packet_len;
    memcpy(packet->data, buffer, packet_len);

    // Populate sender field
    if (connection.type == SOCK_SERVER) {
        packet->sender = fd_to_id(socket_fd);
    } else {
        packet->sender = 0;    // ID 0 is reserved for server
    }

    // Find end of packet queue, and add packet to end
    if (connection.packet_queue == NULL) {
        connection.packet_queue = packet;
    } else {
        Packet* queue_end = connection.packet_queue;
        while (queue_end->next_packet != NULL) {
            queue_end = queue_end->next_packet;
        }
        queue_end->next_packet = packet;
    }

    return SOCK_SUCCESS;
}

SocketState* sock_get_state(void) {

    return &connection;
}

// Return number of packets in queue
int num_packets(void) {
    
    int num_packets = 0;

    Packet* q_ptr = connection.packet_queue;
    while (q_ptr != NULL) {
        num_packets++;
        q_ptr = q_ptr->next_packet;
    }

    return num_packets;
}

// Pop packet at top of packet queue and return pointer. Ownership passes to caller.
Packet* pop_packet(void) {

    Packet* q_ptr = connection.packet_queue;
    if (q_ptr != NULL) {
        connection.packet_queue = connection.packet_queue->next_packet;
        q_ptr->next_packet = NULL;
    }

    return q_ptr;
}            

// Start a server on the local host at specified port
SocketStatus start_server_socket(const char* port) {

    memset(&connection, 0, sizeof connection);   // Clear out state

    int status;             // Variable for storing function return status
    int socket_fd;          // Variable for storing socket file descriptor

    struct addrinfo hints = {0}; // Struct to pass inputs to getaddrinfo
    struct addrinfo *addr, *addr0;       // Struct to get results from getaddrinfo

    // If already initialized, return error
    if (connection.type != SOCK_UNINITIALIZED) return SOCK_ERR_ALREADY_INITIALIZED;

    hints.ai_family = AF_UNSPEC;        // Either IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;    // TCP Stream Socket
    hints.ai_flags = AI_PASSIVE;        // Fill in IP

    status = getaddrinfo(NULL, port, &hints, &addr0);
    if (status != 0 || addr0 == NULL) {
        PRINT_ERROR2("Unable to get address.", gai_strerror(status));
        return SOCK_ERR_SERVER_START_FAILURE;
    }

    // Bind to first address we can
    for (addr = addr0; addr != NULL; addr = addr->ai_next) {

        // Open a streaming socket
        socket_fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (socket_fd == -1) {
            continue;
        }

        // Bind socket to our ip and port
        status = bind(socket_fd, addr->ai_addr, addr->ai_addrlen);
        if (status != 0) {
            close(socket_fd);
            socket_fd = -1;
            continue;
        }

        // If we succeed in binding to socket, exit loop
        break;
    }

    // Free memory allocated for addresses
    freeaddrinfo(addr0);

    // If unable to bind, exit with failure
    if (socket_fd == -1) {
        PRINT_ERROR("Unable to bind to socket.");
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

    // Begin listening for connections on socket, with maximum backlog of 10
    status = listen(socket_fd, 10);
    if (status != 0) {
        PRINT_ERROR("Unable to listen on socket.");
        return SOCK_ERR_SERVER_START_FAILURE;
    }

    // Update type, and save socket fd
    connection.type = SOCK_SERVER;
    connection.socket = socket_fd;

    // Update default id #
    connection.next_id = 1000;

    // Setup our packet queue
    connection.packet_queue = NULL;

    return SOCK_SUCCESS;
}

// Accept any incoming connections, add to client list
SocketStatus accept_client_socket(void) {
    
    int client_socket;
    struct sockaddr_storage cli_addr;
    socklen_t addr_len = sizeof cli_addr;

    if (connection.type == SOCK_UNINITIALIZED) return SOCK_ERR_UNINITIALIZED;
    else if (connection.type == SOCK_CLIENT) return SOCK_ERR_INVALID_CMD;

    // Accept incoming connections
    client_socket = accept(connection.socket, (struct sockaddr *)&cli_addr, &addr_len);
    
    if (client_socket == -1 && errno == EWOULDBLOCK) return SOCK_ERR_NO_NEW_CONNECTIONS;
    else if (client_socket == -1) return SOCK_ERR_SOCKET_DISCONNECT;

    if (connection.num_clients >= MAX_CLIENTS) {
        close(client_socket);
        return SOCK_ERR_TOO_MANY_CONNECTIONS;
    }

    // Add new client to list
    connection.clients[connection.num_clients].id = connection.next_id;
    connection.clients[connection.num_clients].fd = client_socket;
    connection.clients[connection.num_clients].active = ACTIVE;
    connection.num_clients++;
    connection.next_id++;

    printf("[Connecting client id: %d on socket: %d]\n", connection.clients[connection.num_clients-1].id, client_socket);

    return SOCK_SUCCESS;
}

// Close connection to client, mark connection as closed
// Note: client still remains in list until it is flushed
SocketStatus disconnect_client_socket(uint16_t client_id) {

    if (connection.type == SOCK_UNINITIALIZED) return SOCK_ERR_UNINITIALIZED;
    else if (connection.type == SOCK_CLIENT) return SOCK_ERR_INVALID_CMD;

    // Remove from client and fd list by overwriting entry with final entry
    for (int i = 0; i < connection.num_clients; i++) {
        if (client_id == connection.clients[i].id) {

            int socket_fd = connection.clients[i].fd;
            printf("[Disconnecting client id: %d on socket: %d]\n", client_id, socket_fd);

            // Mark socket as inactive
            connection.clients[i].active = INACTIVE;

            // Close socket
            close(connection.clients[i].fd);

            return SOCK_SUCCESS;
        }
    }

    return SOCK_ERR_CLIENT_NOT_FOUND;
}

// Remove inactive clients from list of clients
SocketStatus flush_inactive_client_sockets(void) {

    if (connection.type == SOCK_UNINITIALIZED) return SOCK_ERR_UNINITIALIZED;
    else if (connection.type == SOCK_CLIENT) return SOCK_ERR_INVALID_CMD;

    // Remove from client and fd list by overwriting entry with final entry
    for (int i = connection.num_clients - 1; i >= 0; i--) {
        if (connection.clients[i].active == INACTIVE) {

            DEBUG_PRINT3("Flushing inactive client:", connection.clients[i].id);

            // Overwrite entry with data from last entry
            connection.clients[i] = connection.clients[connection.num_clients - 1];

            // Overwrite last entry with zeroes
            memset(&connection.clients[connection.num_clients - 1], 0, sizeof(struct Client));

            // Decrement number of clients
            connection.num_clients--;
            
        }
        
    }

    return SOCK_SUCCESS;
}

// Send packet to client
SocketStatus server_socket_send_packet(uint16_t client_id, const char* data, size_t num_bytes) {

    return send_packet(id_to_fd(client_id), data, num_bytes);
}

// Receive packet from client
SocketStatus server_socket_recv_packet(uint16_t client_id) {

    return recv_packet(id_to_fd(client_id));
}

// Shutdown server and all client connections
SocketStatus shutdown_server_socket(void) {

    if (connection.type == SOCK_UNINITIALIZED) return SOCK_ERR_UNINITIALIZED;
    else if (connection.type == SOCK_CLIENT) return SOCK_ERR_INVALID_CMD;

    printf("Shutting down connection.\n");
    for (int i = 1; i < connection.num_clients; i++) {
        disconnect_client_socket(connection.clients[i].id);
    }

    close(connection.socket);

    memset(&connection, 0, sizeof(SocketState));

    return SOCK_SUCCESS;
}

// Poll connection for connections or packets
// Accept any new connections, and add new packets to queue
SocketStatus poll_sockets(int timeout) {

    // Poll for any activity
    struct pollfd active_fds[MAX_CLIENTS + 1] = {0};
    uint16_t active_ids[MAX_CLIENTS + 1] = {0};
    int num_active;
    int num_events;
    int status;

    if (connection.type == SOCK_UNINITIALIZED) return SOCK_ERR_UNINITIALIZED;

    // Create list of fds
    num_active = 1;
    active_fds[0].fd = connection.socket;
    active_fds[0].events = POLLIN;
    for (int i = 0; i < connection.num_clients; i++) {
        if (connection.clients[i].active == ACTIVE) {
            active_fds[num_active].fd = connection.clients[i].fd;
            active_fds[num_active].events = POLLIN;
            active_ids[num_active] = connection.clients[i].id; // Store id for future use
            num_active++;
        }
    }

    // Also poll stdin if this is a client
    if (connection.type == SOCK_CLIENT) {
        active_fds[num_active].fd = 0;  // stdin file descriptor
        active_fds[num_active].events = POLLIN;
        num_active++;
    }

    num_events = poll(active_fds, num_active, timeout);

    if (num_events < 0) return SOCK_ERR_POLL_FAILURE;

    if (connection.type == SOCK_SERVER) {
        // First check our connection for any incoming requests
        if (active_fds[0].revents & POLLIN) {
            DEBUG_PRINT("Polled new connection");
            status = accept_client_socket();
            if (status == SOCK_ERR_SOCKET_DISCONNECT) {
                DEBUG_PRINT("Socket error.");
                shutdown_server_socket();
                return SOCK_ERR_SOCKET_DISCONNECT;
            }
        }

        // Now check remaining ports for packets
        for (int i = 1; i < num_active; i++) {
            if (active_fds[i].revents & POLLIN) {
                DEBUG_PRINT("Polled new packet");
                status = recv_packet(active_fds[i].fd);
                if (status == SOCK_ERR_SOCKET_DISCONNECT) {
                    disconnect_client_socket(active_ids[i]);
                }
            }
        }
    } else if (connection.type == SOCK_CLIENT) {
        // Check if our client socket has any packets
        if (active_fds[0].revents & POLLIN) {
            DEBUG_PRINT("Polled new packet");
            status = recv_packet(active_fds[0].fd);
            if (status == SOCK_ERR_SOCKET_DISCONNECT) {
                DEBUG_PRINT("Server disconnected.");
                shutdown_client_socket();
                return SOCK_ERR_SOCKET_DISCONNECT;
            }
        }
    }


    return SOCK_SUCCESS;
}

// Start a client and connect to host at specified port
SocketStatus start_client_socket(const char* host, const char* port) {

    int status;             // Variable for storing function return status
    int socket_fd;           // Variable for storing socket file descriptor

    struct addrinfo hints = {0};    // Struct to pass inputs to getaddrinfo
    struct addrinfo *addr, *addr0;  // Structs to get results from getaddrinfo

    // If already initialized, return error
    if (connection.type != SOCK_UNINITIALIZED) return SOCK_ERR_ALREADY_INITIALIZED;

    hints.ai_family = AF_UNSPEC;        // Either IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;    // TCP Stream Socket
    hints.ai_flags = AI_PASSIVE;        // Fill in IP

    status = getaddrinfo(host, port, &hints, &addr0);
    if (status != 0 || addr0 == NULL) {
        PRINT_ERROR2("Unable to get address.", gai_strerror(status));
        return SOCK_ERR_CLIENT_START_FAILURE;
    }

    // Connect to first address that works
    for (addr = addr0; addr != NULL; addr = addr->ai_next) {

        // Attempt to open streaming socket
        socket_fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (socket_fd == -1) {
            continue;
        }

        // Attempt to connect to socket
        status = connect(socket_fd, addr->ai_addr, addr->ai_addrlen);
        if (status == -1) {
            close(socket_fd);
            socket_fd = -1;
            continue;
        }

        // If we connect successfully, leave loop
        break;
    }

    // Free memory allocated for addresses
    freeaddrinfo(addr0);

    if (socket_fd == -1) {
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

    // Setup our packet queue
    connection.packet_queue = NULL;

    return SOCK_SUCCESS;
}

// Send packet from client to server
SocketStatus client_socket_send_packet(const char* data, size_t num_bytes) {

    if (connection.type != SOCK_CLIENT) return SOCK_ERR_INVALID_CMD;

    return send_packet(connection.socket, data, num_bytes);
}

// Shutdown client
SocketStatus shutdown_client_socket(void) {

    if (connection.type != SOCK_CLIENT) return SOCK_ERR_INVALID_CMD;

    printf("Shutting down client.\n");

    close(connection.socket);

    memset(&connection, 0, sizeof(SocketState));

    return SOCK_SUCCESS;
}
