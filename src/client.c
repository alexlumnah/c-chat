#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <curses.h>
#include <time.h>

#include "sock.h"
#include "chat.h"

ChatClient client;

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

// Send message              
static ChatStatus client_send_message(const MessageHeader* msg) {

    int status;
    char* buffer;
    int num_bytes;
    
    num_bytes = serialize_msg(msg, &buffer);

    if (num_bytes == 0) {
        printf_message("[ERROR] Failed to serialize message");
        return CHAT_FAILURE;
    }

    status = client_socket_send_packet(buffer, num_bytes);
    free(buffer);

    if (status != SOCK_SUCCESS) return CHAT_FAILURE;

    return CHAT_SUCCESS;
}

// Send request to server to set client name      
static ChatStatus client_req_user_setname(const char* username) {

    UserMessage user_msg = {0};
    user_msg.header.type = MSG_USER_SETNAME;
    user_msg.header.from = client.id;
    user_msg.header.to = SERVER_ID;

    user_msg.id = client.id;
    strncpy(user_msg.username, username, MAX_USERNAME_LEN);

    return client_send_message((MessageHeader*)&user_msg);
}

// Ping Server             
static ChatStatus client_ping_server(void) {

    PingMessage ping_msg = {0};
    ping_msg.header.type = MSG_PING;
    ping_msg.header.from = client.id;
    ping_msg.header.to = SERVER_ID;   // Default Server Address

    ping_msg.time = clock();

    return client_send_message((MessageHeader*)&ping_msg);
}

// Send a chat message                
static ChatStatus client_send_chat(uint16_t to, const char* msg_text) {

    ChatMessage chat_msg = {0};
    chat_msg.header.type = MSG_CHAT;
    chat_msg.header.from = client.id;
    chat_msg.header.to = to;

    strncpy(chat_msg.msg, msg_text, MAX_CHATMSG_LEN);

    return client_send_message((MessageHeader*)&chat_msg);

}

// Interpret user input
static void interpret_input(const char* buffer, int buff_len) {

    // If not a command, send message
    if (buffer[0] != '/') {
        client_send_chat(SERVER_ID, buffer);
        return;
    }
    
    // Otherwise parse command
    if (strncmp(&buffer[1], "ping", 4) == 0) {
        client_ping_server();
    } else if (strncmp(&buffer[1], "setname ", 8) == 0) {

        // Check length of username
        if (buff_len - 9 > MAX_USERNAME_LEN) {
            printf_message("Error: Desired username is too long.");
            return;
        }

        client_req_user_setname(&buffer[9]);
    } else {
        // Drop invalid messages
        return;
    }
}

// Update list of all active users in client
static void client_update_active_users(const ActiveUserMessage* msg) {
    
    // Compare client list against server list, and add any new users
    for (int i = 0; i < msg->num_users; i++) {
        if (!check_user_exists(msg->ids[i])) {
            client.users[client.num_users].id = msg->ids[i];
            client.users[client.num_users].active = USER_ACTIVE;
            strncpy(client.users[client.num_users].name, msg->usernames[i], MAX_USERNAME_LEN);
            client.num_users++;
        }
    }

    // Also double check that all clients are in server list
    for (int i = client.num_users - 1; i >= 0; i--) {
        bool user_active = false;
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

}

// Read message, and update chat room state
static void client_handle_packet(Packet* packet) {

    MessageHeader* msg = deserialize_msg(packet->data, packet->len);

    switch (msg->type) {
    case MSG_PING: {
        double time = clock() - ((PingMessage*)msg)->time;
        time *= 1000.0 / CLOCKS_PER_SEC; // Convert time to ms

        printf_message("<PING! - %0.3fms>", time);
        break;
    }
    case MSG_USER_SETNAME: {

        UserMessage* user_msg = (UserMessage*)msg;
        int user_index = get_user_index(user_msg->id);

        if (user_index == -1) {
            printf_message("[ERROR] User id %d doesn't exist.",user_msg->id);
            break;
        }

        strncpy(client.users[user_index].name, user_msg->username, MAX_USERNAME_LEN);
        printf_message("<Updated user %d to %s>",user_msg->id, user_msg->username);
        update_user_display(client.users, client.num_users);
        break;
    }
    case MSG_USER_CONNECT: {

        UserMessage* user_msg = (UserMessage*)msg;
        int user_exists = check_user_exists(user_msg->id);

        if (user_exists) {
            printf_message("[ERROR] User id %d already exists.",user_msg->id);
            break;
        }

        client.users[client.num_users].id = user_msg->id;
        client.users[client.num_users].active = USER_ACTIVE;
        client.num_users++;

        printf_message("<New User %d Connected>",user_msg->id);
        update_user_display(client.users, client.num_users);
        break;
    }
    case MSG_USER_DISCONNECT: {

        UserMessage* user_msg = (UserMessage*)msg;
        int user_index = get_user_index(user_msg->id);

        if (user_index == -1) {
            printf_message("[ERROR] User id %d does not exist.",user_msg->id);
            break;
        }

        // Mark user as inactive
        client.users[user_index].active = USER_INACTIVE;

        printf_message("<User %d Disconnected>",user_msg->id);
        update_user_display(client.users, client.num_users);
        break;    
    }
    case MSG_ACTIVE_USERS:
        printf_message("<Updating active user list>");
        client_update_active_users((ActiveUserMessage*)msg);
        update_user_display(client.users, client.num_users);
        break;
    case MSG_CHAT: {

        // Look up user
        User user;
        int i = get_user_index(msg->from);

        if (i == -1) {
            printf_message("[ERROR] Received message from unknown user.");
            break;
        }

        user = client.users[i];

        // If there is no username, print id, otherwise print name
        if (strnlen(user.name, MAX_USERNAME_LEN) == 0) {
            printf_message("%d: %s",msg->from,((ChatMessage*)msg)->msg);
        } else {
            printf_message("%s: %s",user.name,((ChatMessage*)msg)->msg);
        }
        break;
    }
    case MSG_ERROR:
        printf_message("[ERROR]: %s",((ErrorMessage*)msg)->msg);
        break;
    default:
        printf_message("[ERROR] Received invalid message type.");
        break;
    }

    free(msg);
}

// Check for message from socket
static ChatStatus client_check_messages(int timeout) {

    int status;
    Packet* packet;

    status = poll_sockets(timeout);

    if (status != SOCK_SUCCESS) return CHAT_FAILURE;

    // Check for messages
    packet = pop_packet();

    // Handle Packet
    if (packet != NULL) {
        client_handle_packet(packet);
        free(packet);
    }

    return CHAT_SUCCESS;
}

// Start chat client
ChatStatus start_chat_client(const char* host, const char* port) {

    int status;
    Packet* packet;
    MessageHeader* msg;

    status = start_client_socket(host, port);

    if (status != SOCK_SUCCESS) return CHAT_FAILURE;
    
    printf("Client started. Listening for server greeting...\n");

    // 10 second timeout
    status = poll_sockets(10000);

    if (status != SOCK_SUCCESS) return CHAT_FAILURE;

    packet = pop_packet();
    if (packet == NULL) {
        printf("No reply from server. Disconnecting.\n");
        return CHAT_FAILURE;
    }

    // Expect first message to be list of active users
    msg = deserialize_msg(packet->data, packet->len);
    if (msg->type != MSG_ACTIVE_USERS) {
        printf("Incorrect greeting from server. Disconnecting.\n");
        free(msg);
        free(packet);
        return CHAT_FAILURE;
    }
    
    client_update_active_users((ActiveUserMessage*)msg);

    // Capture client id
    client.id = msg->to;

    printf("Client connected successfully. Client id: %d\n", client.id);

    free(msg);
    free(packet);

    return CHAT_SUCCESS;
}

// Run client until escape key is pressed, or server disconnects.
ChatStatus client_run(void) {

    int c;
    int status;
    int buff_len = 0;
    char buffer[MAX_MESSAGE_LEN] = {0};

    // Initialize UI
    init_window();

    // Add users to UI
    update_user_display(client.users, client.num_users);

    // Core loop - listen for inputs and messages
    do {

        // Poll for messages
        status = client_check_messages(1000);

        // Get input
        while ((c = getch()) != ERR){

            // Store any ascii characters in buffer
            if (c >= ' ' && c <= '~' && buff_len < MAX_MESSAGE_LEN - 1) {
                buffer[buff_len] = c;
                buff_len++;

            // Handle backspaces
            } else if ((c == KEY_BACKSPACE || c == 127 || c == '\b') && buff_len > 0) {
                buff_len--;
                buffer[buff_len] = 0;

            // Interpret input when Enter or Return is pressed
            } else if (c == KEY_ENTER || c == '\n') {
                interpret_input(buffer, buff_len);
                memset(buffer, 0, buff_len);
                buff_len = 0;

            // Reset UI when terminal is resized
            } else if (c == KEY_RESIZE) {
                update_user_display(client.users, client.num_users);

            // Break out of loop when ESC key is detected
            } else if (c == 27) {
                break;
            }

        } 

        // Update screen
        draw_buffer(buffer);

    } while (c != 27 && status != CHAT_FAILURE);

    return status;

}

void end_chat_client(void) {

    // Kill UI
    kill_window();

    // Shutdown socket
    shutdown_client_socket();

}
