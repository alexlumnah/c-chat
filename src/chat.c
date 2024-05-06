#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>

#include "chat.h"
#include "sock.h"

ChatRoom chat;

// Serialize message, typecast message into header, function will alloc required memory
// Return number of bytes in buffer, return 0 on error
size_t serialize_msg(MessageHeader* msg, char** buffer) {

    uint16_t msg_len = 0;

    switch (msg->type) {
    case MSG_PING:
        *buffer = calloc(1, 7); // Message header only requires 7 bytes
        if (*buffer == NULL) return 0;
        msg_len = 7;
        break;
    case MSG_USER_SETNAME:      // Intentional fall through
    case MSG_USER_CONNECT:      // Intentional fall through
    case MSG_USER_DISCONNECT: {

        // Get relevant data
        UserMessage* user_msg = (UserMessage*)msg;
        uint16_t id = htons(user_msg->id);

        // Allocate buffer memory, and create pointer to end of header data
        *buffer = calloc(1, sizeof(UserMessage));
        if (*buffer == NULL) return 0;
        char* buff_ptr = *buffer + 7;   // Leave room for header

        // Serialize id
        memcpy(buff_ptr, &id, sizeof(uint16_t));
        buff_ptr = buff_ptr + sizeof(uint16_t);
        
        // Serialize username
        strncat(buff_ptr, user_msg->username, MAX_USERNAME_LEN);
        buff_ptr += strnlen(user_msg->username, MAX_USERNAME_LEN) + 1;  // Remember null byte

        // Calculate number of bytes and trim buffer down to size
        msg_len = buff_ptr - *buffer;
        *buffer = realloc(*buffer, msg_len);

        break;
    }
    case MSG_ACTIVE_USERS: {

        // Get relevant data
        ActiveUserMessage* user_msg = (ActiveUserMessage*)msg;
        uint8_t num_users = user_msg->num_users;
        uint16_t id;

        // Allocate buffer memory, and create pointer to end of header data
        *buffer = calloc(1, sizeof(ActiveUserMessage));
        if (*buffer == NULL) return 0;
        char* buff_ptr = *buffer + 7;   // Leave room for header

        // Serialize number of users
        *buff_ptr = num_users;
        buff_ptr++;

        // Now serialize array of user ids
        for (int i = 0; i < num_users; i++) {
            id = htons(user_msg->ids[i]);
            memcpy(buff_ptr, &id, 2);
            buff_ptr += 2;
        }

        // And serialize array of user names
        for (int i = 0; i < num_users; i++) {
            strncat(buff_ptr, user_msg->usernames[i], MAX_USERNAME_LEN);
            buff_ptr += strnlen(user_msg->usernames[i], MAX_USERNAME_LEN) + 1;  // Remember null byte
        }
        
        // Calculate number of bytes and trim buffer down to size
        msg_len = buff_ptr - *buffer;
        *buffer = realloc(*buffer, msg_len);

        break;
    }
    case MSG_CHAT: {
        
        // Get relevant data
        ChatMessage* chat_msg = (ChatMessage*)msg;

        // Allocate buffer memory, and create pointer to end of header data
        *buffer = calloc(1, sizeof(ChatMessage));
        if (*buffer == NULL) return 0;
        char* buff_ptr = *buffer + 7;   // Leave room for header

        // Serialize chat message
        strncat(buff_ptr, chat_msg->msg, MAX_CHATMSG_LEN);
        buff_ptr += strnlen(chat_msg->msg, MAX_CHATMSG_LEN) + 1;  // Remember null byte
        
        // Calculate number of bytes and trim buffer down to size
        msg_len = buff_ptr - *buffer;
        *buffer = realloc(*buffer, msg_len);

        break;
    }
    }

    // Now fill in header information
    uint16_t nw_len = htons(msg_len);       // Message Length
    uint16_t nw_from = htons(msg->from);    // From
    uint16_t nw_to = htons(msg->to);        // To

    *buffer[0] = (uint8_t)msg->type;        // Type
    memcpy(&((*buffer)[1]), &nw_len, 2);    // Length
    memcpy(&((*buffer)[3]), &nw_from, 2);   // From
    memcpy(&((*buffer)[5]), &nw_to, 2);     // To

    return msg_len;
}

// Deserialize a message, function will malloc required memory and store in message pointer
// Return pointer to deserialized message. Ownership passess to caller.
MessageHeader* deserialize_msg(char* buffer, int buffer_size) {

    // Allocate memory for a message header
    MessageHeader* msg = calloc(1, sizeof(MessageHeader));
    if (msg == NULL) return NULL;

    // Deserialize the header
    char* buff_ptr = buffer;
    msg->type = (MessageType)buff_ptr[0];   // Type
    buff_ptr++;

    // Message Length
    memcpy(&msg->len, buff_ptr, 2);         
    msg->len = (uint16_t)ntohs(msg->len);   // Ensure length is host-endian
    buff_ptr += 2;

    // Message From
    memcpy(&msg->from, buff_ptr, 2);         
    msg->from = (uint16_t)ntohs(msg->from);   // Ensure length is host-endian
    buff_ptr += 2;

    // Message To
    memcpy(&msg->to, buff_ptr, 2);         
    msg->to = (uint16_t)ntohs(msg->to);   // Ensure length is host-endian
    buff_ptr += 2;

    switch (msg->type) {
    case MSG_PING:
        return msg;
    case MSG_USER_SETNAME:      // Intentional fall through
    case MSG_USER_CONNECT:      // Intentional fall through
    case MSG_USER_DISCONNECT: {

        // Reallocate enough room for this message, set to all zeroes
        UserMessage* user_msg = (UserMessage*)realloc(msg, sizeof(UserMessage));
        if (user_msg == NULL) return NULL;
        memset((char*)user_msg + sizeof(MessageHeader), 0, sizeof(UserMessage) - sizeof(MessageHeader));

        // Deserialize id
        memcpy(&user_msg->id, buff_ptr, 2);
        user_msg->id = (uint16_t)ntohs(user_msg->id);
        buff_ptr += 2;
        
        // Deserialize username
        strncat(user_msg->username, buff_ptr, MAX_USERNAME_LEN);
        buff_ptr += strnlen(user_msg->username, MAX_USERNAME_LEN) + 1; // Remember null byte

        return (MessageHeader*)user_msg;
        
    }
    case MSG_ACTIVE_USERS: {

        // Reallocate enough room for this message, set to all zeroes
        ActiveUserMessage* user_msg = (ActiveUserMessage*)realloc(msg, sizeof(ActiveUserMessage));
        if (user_msg == NULL) return NULL;
        memset((char*)user_msg + sizeof(MessageHeader), 0, sizeof(ActiveUserMessage) - sizeof(MessageHeader));

        // Deserialize number of users
        user_msg->num_users = (uint8_t)buff_ptr[0];
        buff_ptr++;

        // Now deserialize array of user ids
        for (int i = 0; i < user_msg->num_users; i++) {
            memcpy(&(user_msg->ids[i]), buff_ptr, 2);
            user_msg->ids[i] = (uint16_t)ntohs(user_msg->ids[i]);
            buff_ptr += 2;
        }

        // And serialize array of user names
        for (int i = 0; i < user_msg->num_users; i++) {
            strncat(user_msg->usernames[i], buff_ptr, MAX_USERNAME_LEN);
            buff_ptr += strnlen(user_msg->usernames[i], MAX_USERNAME_LEN) + 1;  // Remember null byte
        }

        return (MessageHeader*)user_msg;
    }
    case MSG_CHAT: {
    
        // Reallocate enough room for this message, set to all zeroes
        ChatMessage* chat_msg = (ChatMessage*)realloc(msg, sizeof(ChatMessage));
        if (chat_msg == NULL) return NULL;
         memset((char*)chat_msg + sizeof(MessageHeader), 0, sizeof(ChatMessage) - sizeof(MessageHeader));
            
        // Deserialize string
        strncat(chat_msg->msg, buff_ptr, MAX_CHATMSG_LEN);
        buff_ptr += strnlen(chat_msg->msg, MAX_CHATMSG_LEN) + 1; // Remember null byte
    
        return (MessageHeader*)chat_msg;
    }
    }

    return 0;
}

// Start chat server, and run until disconnected
int start_chat_server(char* port);    

// Run chat server, poll for requests, and forward messages                  
int chat_server_run(void);               

// Send set name request to all users               
int server_send_user_setname(uint16_t id, char* name);

// Send user connect message to all users  
int server_send_user_connect(uint16_t id);        

// Send user disconnect message to all users      
int server_send_user_disconnect(uint16_t id);   

// Send list of all active users to all users        
int server_send_active_users(void);        

// Ping a client             
int server_ping(uint16_t id);                                  

// Start chat client
int start_chat_client(char* host, char* port);  

// Read message, and update chat room state        
int client_read_message(MessageHeader* msg);     

// Send request to server to set client name      
int client_req_user_setname(char* username);

// Request all active users from server
int client_req_active_users(void);         

// Ping Server             
int client_ping(void);                  

// Send message to entire chat                
int client_send_all(ChatMessage* msg);                  
