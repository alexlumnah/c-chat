#ifndef CHAT_H
#define CHAT_H

#include <stddef.h>
#include "sock.h"

#define MAX_USERNAME_LEN (16)
#define MAX_CHATMSG_LEN  (255)

typedef enum MessageType {
    MSG_PING,
    MSG_USER_SETNAME,
    MSG_USER_CONNECT,
    MSG_USER_DISCONNECT,
    MSG_ACTIVE_USERS,
    MSG_CHAT,
} MessageType;

typedef struct MessageHeader {
    uint8_t type;                       // Message type   
    uint16_t len;                       // Length of proceeding data, populated by serialize function
    uint16_t from;                      // Message Source
    uint16_t to;                        // Message Destination
} MessageHeader;

typedef MessageHeader PingMessage;

typedef struct UserMessage {
    MessageHeader header;
    uint16_t id;
    char username[MAX_USERNAME_LEN + 1];
} UserMessage;

typedef struct ActiveUserMessage {
    MessageHeader header;
    uint8_t num_users;
    uint16_t ids[MAX_CLIENTS];
    char usernames[MAX_CLIENTS][MAX_USERNAME_LEN + 1];
} ActiveUserMessage;

typedef struct ChatMessage {
    MessageHeader header;
    char msg[MAX_CHATMSG_LEN + 1];
} ChatMessage;

typedef enum UserStatus {
    USER_INACTIVE = 0,
    USER_ACTIVE = 1,
} UserStatus;

typedef struct User {
    uint16_t id;
    UserStatus active;
    char name[MAX_USERNAME_LEN + 1];
} User;

typedef struct ChatRoom {
    int num_users;
    User users[MAX_CLIENTS];
    ChatMessage* messages;
} ChatRoom;

// General utilities
size_t serialize_msg(MessageHeader* msg, char** buffer);        // Serialize message, typecast message into header, function will malloc required memory
MessageHeader* deserialize_msg(char* buffer, int buffer_size);   // Deserialize a message, function will malloc required memory

// Server utilties
int start_chat_server(char* port);                      // Start chat server, and run until disconnected
int chat_server_run(void);                              // Run chat server, poll for requests, and forward messages
int server_send_user_setname(uint16_t id, char* name);  // Send set name request to all users
int server_send_user_connect(uint16_t id);              // Send user connect message to all users
int server_send_user_disconnect(uint16_t id);           // Send user disconnect message to all users
int server_send_active_users(void);                     // Send list of all active users to all users
int server_ping(uint16_t id);                           // Ping a client

// Chat Room Utilties
int start_chat_client(char* host, char* port);          // Start chat client
int client_read_message(MessageHeader* msg);            // Read message, and update chat room state
int client_req_user_setname(char* username);            // Send request to server to set client name
int client_req_active_users(void);                      // Request all active users from server
int client_ping(void);                                  // Ping Server
int client_send_all(ChatMessage* msg);                  // Send message to entire chat

// Future functionality -> send direct message

#endif // CHAT_H
