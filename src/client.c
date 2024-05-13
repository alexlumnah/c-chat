#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "sock.h"
#include "chat.h"

ChatClient client;

ChatClient* get_client(void) {
    return &client;
}

// Start chat client
int start_chat_client(char* host, char* port) {

    int status;
    Packet* packet;
    MessageHeader* msg;

    status = start_client(host, port);

    if (status != SOCK_SUCCESS) return -1;
    
    printf("Client started. Listening for server reply.\n");

    // 10 second timeout
    status = poll_sockets(10000);

    if (status != SOCK_SUCCESS) return -1;

    packet = pop_packet();
    if (packet == NULL) {
        printf("No reply from server. Disconnecting.\n");
        return -1;
    }

    // Expect first message to be list of active users
    msg = deserialize_msg(packet->data, packet->len);
    if (msg->type != MSG_ACTIVE_USERS) {
        printf("Incorrect greeting from server. Disconnecting.\n");
        free(msg);
        free(packet);
        return -1;
    }
    
    client_update_active_users((ActiveUserMessage*)msg);

    // Capture client id
    client.id = msg->to;

    printf("Client connected successfully. Client id: %d\n", client.id);

    free(msg);
    free(packet);

    return 1;
}

// Check for message from socket
int client_check_messages(int timeout) {

    int status;
    Packet* packet;

    status = poll_sockets(timeout);

    if (status != SOCK_SUCCESS) return -1;

    // Check for messages
    packet = pop_packet();

    // Handle Packet
    if (packet != NULL) {
        status = client_handle_packet(packet);
        free(packet);
    }

    if (status != 1) return -1;

    return 1;
}

// Get index of user in user list
static int get_user_index(uint16_t id) {

    for (int i = 0; i < client.num_users; i++) {
        if (client.users[i].id == id) {
            return i;
        }
    }

    return -1;
}
// Check if a user exists
static bool check_user_exists(uint16_t id) {

    return get_user_index(id) != -1;
    
}

// Update list of all active users in client
int client_update_active_users(ActiveUserMessage* msg) {
    
    // Compare client list against server list, and add any new users
    for (int i = 0; i < msg->num_users; i++) {
        if (!check_user_exists(msg->ids[i])) {
            client.users[client.num_users].id = msg->ids[i];
            client.users[client.num_users].active = USER_ACTIVE;
            strncat(client.users[client.num_users].name, msg->usernames[i], MAX_USERNAME_LEN);
            client.num_users++;
            printf("[DEBUG] Adding user %d with username: %s\n",msg->ids[i],msg->usernames[i]);
        }
    }

    // Also double check that all clients are in server list
    bool user_active;
    for (int i = client.num_users - 1; i >= 0; i--) {
        user_active = false;
        for (int j = 0; j < msg->num_users; j++) {
            if (client.users[i].id == msg->ids[j]) {
                user_active = true;
                break;
            }
        }
        // If user isn't in active list, overwrite it with the last user
        if (!user_active) {
            client.num_users--;
            client.users[i] = client.users[client.num_users];
            client.users[client.num_users] = (User){0};
        }
    }

    return 1;
}


// Read message, and update chat room state        
int client_handle_packet(Packet* packet) {

    MessageHeader* msg = deserialize_msg(packet->data, packet->len);

    switch (msg->type) {
    case MSG_PING:
        printf("PING!");
        break;
    case MSG_USER_SETNAME: {

        UserMessage* user_msg = (UserMessage*)msg;
        int user_index = get_user_index(user_msg->id);

        if (user_index == -1) {
            printf("[ERROR] User id %d doesn't exist.",user_msg->id);
            free(msg);
            return -1;
        }

        strncat(client.users[user_index].name, user_msg->username, MAX_USERNAME_LEN);
        printf("[DEBUG] Updated user %d to %s\n",user_msg->id, user_msg->username);
        break;
    }
    case MSG_USER_CONNECT: {

        UserMessage* user_msg = (UserMessage*)msg;
        int user_exists = check_user_exists(user_msg->id);

        if (user_exists) {
            printf("[ERROR] User id %d already exists.",user_msg->id);
            free(msg);
            return -1;
        }

        client.users[client.num_users].id = user_msg->id;
        client.users[client.num_users].active = USER_ACTIVE;
        client.num_users++;

        printf("[DEBUG] New user %d\n",user_msg->id);
        break;
    }
    case MSG_USER_DISCONNECT: {

        UserMessage* user_msg = (UserMessage*)msg;
        int user_index = get_user_index(user_msg->id);

        if (user_index == -1) {
            printf("[ERROR] User id %d does not.",user_msg->id);
            free(msg);
            return -1;
        }

        // Mark user as inactive
        client.users[user_index].active = USER_INACTIVE;

        printf("[DEBUG] User Disconnected %d\n",user_msg->id);
        break;    
    }
    case MSG_ACTIVE_USERS:
        printf("[DEBUG] Updating active users\n");
        client_update_active_users((ActiveUserMessage*)msg);
        break;
    case MSG_CHAT:
        // Capture chat in message history
        client.msgs[client.num_msgs] = *(ChatMessage*)msg;
        client.num_msgs++;
        printf("[DEBUG] New message from %d: %s", msg->from, ((ChatMessage*)msg)->msg);
        break;
    default:
        printf("[ERROR] Invalid message type\n");
        break;
    }

    free(msg);
    return 1;
}

// Send request to server to set client name      
int client_req_user_setname(char* username) {

    printf("Requesting setname to %s\n", username);
    UserMessage user_msg = {0};
    user_msg.header.type = MSG_USER_SETNAME;
    user_msg.header.from = client.id;
    user_msg.header.to = SERVER_ID;

    user_msg.id = client.id;
    strncat(user_msg.username, username, MAX_USERNAME_LEN);

    return client_send_message((MessageHeader*)&user_msg);
}

// Request all active users from server
int client_req_active_users(void) {

    UserMessage user_msg = {0};
    user_msg.header.type = MSG_ACTIVE_USERS;
    user_msg.header.from = client.id;
    user_msg.header.to = SERVER_ID;

    return client_send_message((MessageHeader*)&user_msg);
}        


// Ping Server             
int client_ping_server(void) {

    PingMessage ping_msg = {0};
    ping_msg.header.type = MSG_PING;
    ping_msg.header.from = client.id;
    ping_msg.header.to = SERVER_ID;   // Default Server Address

    return client_send_message((MessageHeader*)&ping_msg);
}

// Send a chat message                
int client_send_chat(uint16_t to, char* msg_text) {

    ChatMessage chat_msg = {0};
    chat_msg.header.type = MSG_CHAT;
    chat_msg.header.from = client.id;
    chat_msg.header.to = to;

    strncat(chat_msg.msg, msg_text, MAX_CHATMSG_LEN);

    return client_send_message((MessageHeader*)&chat_msg);

}

// Send message              
int client_send_message(MessageHeader* msg) {

    int status;
    char* buffer;
    int num_bytes;
    
    num_bytes = serialize_msg(msg, &buffer);

    if (num_bytes == 0) {
        printf("[ERROR] Failed to serialize message\n");
        return -1;
    }

    status = client_send_packet(buffer, num_bytes);
    free(buffer);

    return status;
}
