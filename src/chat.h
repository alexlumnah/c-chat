#ifndef CHAT_H
#define CHAT_H

#include <stddef.h>
#include "sock.h"

#define MAX_USERNAME_LEN (16)
#define MAX_CHAT_HISTORY (255)
#define MAX_CHATMSG_LEN  (255)
#define SERVER_ID (0)

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

typedef struct PingMessage {
    MessageHeader header;
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

typedef enum UserStatus {
    USER_INACTIVE = 0,
    USER_ACTIVE = 1,
} UserStatus;

typedef struct User {
    uint16_t id;
    UserStatus active;
    char name[MAX_USERNAME_LEN + 1];
} User;

typedef struct ChatServer {
    int num_users;
    User users[MAX_CLIENTS];
    SockState* socket_connection;
} ChatServer;

typedef struct ChatClient {
    uint16_t id;
    char name[MAX_USERNAME_LEN + 1];
    int num_users;
    User users[MAX_CLIENTS];
    ChatMessage msgs[MAX_CHAT_HISTORY];
    int num_msgs;
} ChatClient;


// General utilities
int serialize_msg(MessageHeader* msg, char** buffer);           // Serialize message, typecast message into header, function will malloc required memory
MessageHeader* deserialize_msg(char* buffer, int num_bytes);    // Deserialize a message, function will malloc required memory

// Server utilties
ChatClient* get_client(void);                           // Get pointer to chat client struct
int start_chat_server(char* port);                      // Start chat server, and run until disconnected
int chat_server_run(void);                              // Run chat server, poll for requests, and forward messages
int server_handle_packet(Packet* packet);               // Handle packet from clients
int server_sync_users(void);                            // Sync users between socket connection and server
int server_send_message(MessageHeader* msg);            // Send message based to client addressed in message header
int server_send_user_setname(uint16_t id, char* name);  // Send set name request to all users
int server_send_user_connect(uint16_t id);              // Send user connect message to all users
int server_send_user_disconnect(uint16_t id);           // Send user disconnect message to all users
int server_send_active_users(uint16_t to);              // Send list of all active users to id

// Chat Client Utilties
int start_chat_client(char* host, char* port);          // Start chat client
int client_check_messages(int timeout);                 // Poll and handle new messages
int client_update_active_users(ActiveUserMessage* msg); // Update client list of active users with results of message
int client_handle_packet(Packet* packet);               // Handle packet, and update chat room state
int client_send_message(MessageHeader* msg);            // Send a message to the server
int client_req_user_setname(char* username);            // Send request to server to set client name
int client_req_active_users(void);                      // Request all active users from server
int client_ping_server(void);                           // Ping Server
int client_send_chat(uint16_t to, char* msg);           // Send message to entire chat, expects null terminated string

#endif // CHAT_H
