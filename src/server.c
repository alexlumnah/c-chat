#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "chat.h"
#include "sock.h"

ChatServer server;

// Get index of user in user list
static int get_user_index(uint16_t id) {

    for (int i = 0; i < server.num_users; i++) {
        if (server.users[i].id == id) {
            return i;
        }
    }

    return -1;
}

// Check if a user exists
static bool check_user_exists(uint16_t id) {

    return get_user_index(id) != -1;
    
}

// Check if a username exists
static bool username_taken(const char* username) {

    for (int i = 0; i < server.num_users; i++) {
        if (strncmp(server.users[i].name, username, MAX_USERNAME_LEN) == 0){
            return true;
        }
    }

    return false;
}

// Send a message
static ChatStatus server_send_message(const MessageHeader* msg) {

    int status;
    char* buffer;
    int num_bytes;
    
    num_bytes = serialize_msg(msg, &buffer);

    if (num_bytes == 0) return CHAT_FAILURE;

    if (msg->to == SERVER_ID) {
        status = 0;
        for (int i = 0; i < server.num_users; i++) {
            status = server_socket_send_packet(server.users[i].id, buffer, num_bytes);
        }
    } else {
        status = server_socket_send_packet(msg->to, buffer, num_bytes);
    }
    
    free(buffer);

    return status;
}

// Send set name request to all users               
static ChatStatus server_send_user_setname(uint16_t id, const char* name) {

    UserMessage user_msg = {0};
    user_msg.header.type = MSG_USER_SETNAME;
    user_msg.header.from = SERVER_ID;
    user_msg.header.to = SERVER_ID; // ALL

    user_msg.id = id;
    strncpy(user_msg.username, name, MAX_USERNAME_LEN);

    return server_send_message((MessageHeader*)&user_msg);

}

// Send user connect message to all users  
static ChatStatus server_send_user_connect(uint16_t id) {

    UserMessage user_msg = {0};
    user_msg.header.type = MSG_USER_CONNECT;
    user_msg.header.from = SERVER_ID;
    user_msg.header.to = SERVER_ID; // ALL

    user_msg.id = id;

    return server_send_message((MessageHeader*)&user_msg);

}        

// Send user disconnect message to all users      
static ChatStatus server_send_user_disconnect(uint16_t id) {

    UserMessage user_msg = {0};
    user_msg.header.type = MSG_USER_DISCONNECT;
    user_msg.header.from = SERVER_ID;
    user_msg.header.to = SERVER_ID; // ALL

    user_msg.id = id;

    return server_send_message((MessageHeader*)&user_msg);
}   

// Send list of all active users to all users        
static ChatStatus server_send_active_users(uint16_t id) {

    printf("Broadcasting %d active users.\n", server.num_users);
 
    ActiveUserMessage msg = {0};

    // Build active user message
    msg.header.type = MSG_ACTIVE_USERS;
    msg.header.from = SERVER_ID;
    msg.header.to = id;

    msg.num_users = server.num_users;

    for (int i = 0; i < server.num_users; i++) {
        msg.ids[i] = server.users[i].id;
        strncpy(msg.usernames[i], server.users[i].name, MAX_USERNAME_LEN);
    }

    return server_send_message((MessageHeader*)&msg);

}

// Send error message to user      
static int server_send_error(uint16_t id, const char* err) {

    ErrorMessage err_msg = {0};
    err_msg.header.type = MSG_ERROR;
    err_msg.header.from = SERVER_ID;
    err_msg.header.to = id;

    strncpy(err_msg.msg, err, MAX_CHATMSG_LEN);

    return server_send_message((MessageHeader*)&err_msg);
}    

// Check for new connections and disconnections
static void server_sync_users(void) {

    // Iterate over all connected clients
    for (int i = 0; i < server.socket_connection->num_clients; i++) {

        uint16_t user_id = server.socket_connection->clients[i].id;
        int user_index = get_user_index(user_id);
        bool user_exists = check_user_exists(user_id);
        bool user_active = server.socket_connection->clients[i].active;

        // If user isn't in chat, broadcast connection and update user list
        if (!user_exists && user_active) {
            // Let clients know user is connected
            server_send_user_connect(user_id);
            server.users[server.num_users].id = user_id;
            server.users[server.num_users].active = USER_ACTIVE;
            server.num_users++;
            // Send list of active users to new client
            server_send_active_users(user_id);

        // If user is in chat but leaves, update user list then broadcast
        } else if (user_exists && !user_active) {

            // Remove user from user list by overwriting with last value
            server.num_users--;
            server.users[user_index] = server.users[server.num_users];
            server.users[server.num_users] = (User){0};
            server_send_user_disconnect(user_id);
        }
    }

    // Flush inactive clients from SocketConnection
    flush_inactive_client_sockets();
}

// Handle an incoming message
static void server_handle_packet(Packet* packet) {

    MessageHeader* msg = deserialize_msg(packet->data, packet->len);

    printf("Handling message of type: %d\n", msg->type);

    switch (msg->type) {
    case MSG_PING: {

        // Update message metadata, then reply back with a ping
        printf("PING!\n");
        msg->to = packet->sender;
        msg->from = SERVER_ID;
        server_send_message(msg);
        break;
    }
    case MSG_USER_SETNAME: {

        // First confirm user exists
        int user_index = get_user_index(packet->sender);
        if (user_index == -1) {
            printf("[ERROR] Received packet from unknown sender\n");
            break;
        }

        // Confirm username isn't taken
        if (username_taken(((UserMessage*)msg)->username)) {
            printf("User id: %d requested taken username: %s\n",packet->sender,((UserMessage*)msg)->username);
            server_send_error(packet->sender, "Username already taken.");
            break;
        }

        printf("Setting name of id %d to: %s\n",packet->sender,((UserMessage*)msg)->username);
        strncpy(server.users[user_index].name, ((UserMessage*)msg)->username, MAX_USERNAME_LEN);
        server_send_user_setname(packet->sender, server.users[user_index].name);
        break;
    }
    case MSG_ACTIVE_USERS:
        server_send_active_users(packet->sender);
        break;
    case MSG_CHAT: {
        // Forward chat message to destination
        printf("Forwarding chat to id: %d\n",msg->to);
        if (msg->to == SERVER_ID) {
            for (int i = 0; i < server.num_users; i++) {
                server_socket_send_packet(server.users[i].id, packet->data, packet->len);
            }
        } else {
            server_socket_send_packet(msg->to, packet->data, packet->len);
        }
        break;
    }
    default:
        printf("[ERROR] Invalid message type.\n");
        break;
    }

    free(msg);

}    

// Start chat server, and run until disconnected
ChatStatus start_chat_server(const char* port) {

    int status;

    status = start_server_socket(port);

    if (status != SOCK_SUCCESS) return CHAT_FAILURE;

    server.socket_connection = sock_get_state();

    return CHAT_SUCCESS;
}  

// Run chat server indefinitely, poll for requests, and forward messages                  
void chat_server_run(void) {

    int status;

    do {

        Packet* packet;

        // Poll for inputs, timeout of one second
        status = poll_sockets(1000);

        // Check for new connections and disconnections
        server_sync_users();

        // Check for messages
        packet = pop_packet();

        // Handle Packet
        while (packet != NULL) {
            server_handle_packet(packet);
            free(packet);
            packet = pop_packet();
        }

    } while (status == SOCK_SUCCESS);

}
