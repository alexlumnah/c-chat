#ifndef CHAT_H
#define CHAT_H

#include <stddef.h>

#include "sock.h"

#define MAX_USERNAME_LEN (16)
#define MAX_CHATMSG_LEN  (255)
#define SERVER_ID (0)

typedef enum MessageType {
    MSG_PING,
    MSG_USER_SETNAME,
    MSG_USER_CONNECT,
    MSG_USER_DISCONNECT,
    MSG_ACTIVE_USERS,
    MSG_CHAT,
    MSG_ERROR,
} MessageType;

typedef struct MessageHeader {
    uint8_t type;                           // Message type   
    uint16_t len;                           // Length of proceeding data, populated by serialize function
    uint16_t from;                          // Message Source
    uint16_t to;                            // Message Destination
} MessageHeader;

typedef struct PingMessage {
    MessageHeader header;
    uint32_t time;
} PingMessage;

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

typedef struct ErrorMessage {
    MessageHeader header;
    char msg[MAX_CHATMSG_LEN + 1];
} ErrorMessage;

typedef enum UserStatus {
    USER_INACTIVE = 0,
    USER_ACTIVE = 1,
} UserStatus;

typedef struct User {
    uint16_t id;
    UserStatus active;
    char name[MAX_USERNAME_LEN + 1];
} User;

typedef enum ChatStatus {
    CHAT_SUCCESS = 0,                       // Client/Server Return Sucess
    CHAT_FAILURE,                           // Client/Server Return Failure
} ChatStatus;

typedef struct ChatServer {
    int num_users;                          // Number of users connected to server
    User users[MAX_CLIENTS];                // Array of users connected to server
    SocketState* socket_connection;         // Pointer to socket interface
} ChatServer;

typedef struct ChatClient {
    uint16_t id;                            // id of this client
    char name[MAX_USERNAME_LEN + 1];        // Username of this client
    int num_users;                          // Number of users in chat room
    User users[MAX_CLIENTS];                // List of users in chat room
} ChatClient;


// serial.c: Serialize/Deserialize Messages
int serialize_msg(const MessageHeader* msg, char** buffer);         // Serialize message, typecast message into header, function will malloc required memory
MessageHeader* deserialize_msg(char* buffer, int num_bytes);        // Deserialize a message, function will malloc required memory

// server.c: Server Utilties
ChatStatus start_chat_server(const char* port);                     // Start chat server, and run until disconnected
void chat_server_run(void);                                         // Run chat server, poll for requests, and forward messages

// client.c: Chat Client Utilties
ChatStatus start_chat_client(const char* host, const char* port);   // Start chat client
ChatStatus client_run(void);                                        // Start main chat client loop
void end_chat_client(void);                                         // End chat client

// ui.c: UI Utilities
void init_window(void);                                         // Initialize UI Window
void kill_window(void);                                         // Kill UI Window
void update_user_display(const User* users, int num_users);     // Update User Display
void printf_message(const char* fmt, ...);                      // Print Message to screen
void draw_screen(const char* buffer);                           // Draw Screen

#endif // CHAT_H
