#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <arpa/inet.h>

#include "chat.h"
#include "sock.h"

typedef struct DataBuffer {
    char* buffer;       // Pointer to start of buffer
    int size;           // Length of buffer
    char* ptr;          // Pointer to current location in buffer
} DataBuffer;

// Initialize data buffer.  Allocates memory, ownership passes to caller.
static int init_empty_buff(DataBuffer* db, int size) {
    // Allocate memory for buffer
    db->buffer = calloc(1, size);

    if (db->buffer == NULL) return -1;

    db->ptr = db->buffer;
    db->size = size;

    return 1;
}

// Increment data buffer pointer
static int inc_db(DataBuffer* db, int num_bytes) {
    if ((db->ptr + num_bytes) <= (db->buffer + db->size)) {
        db->ptr += num_bytes;
        return 1;
    }
    return -1;
}

// Return true if there is enough room in buffer for num_bytes
static bool db_has_room(DataBuffer* db, int num_bytes) {
    return (db->ptr + num_bytes) <= (db->buffer + db->size);
}

// Return number of bits up to and including next null byte in data buffer
// Return -1 if buffer ends without null byte, or exceeds max str len
static int db_strnlen(DataBuffer* db, int max_len) {

    // int len = strnlen(db->ptr, db->buffer + db->size - db->ptr) + 1;

    // Iterate through list until you hit a null byte or end of buffer
    char* str_ptr = db->ptr;
    while (str_ptr < (db->buffer + db->size) && (str_ptr - db->ptr) < max_len) {

        // If we find a null byte, return
        if (str_ptr[0] == 0) return (str_ptr - db->ptr + 1); // +1 to include last byte

        // Otherwise increment pointer and continue
        str_ptr++;
    }

    return -1;

}

// Serialize message, typecast message into header, function will alloc required memory
// Return number of bytes in buffer, return -1 on error
int serialize_msg(MessageHeader* msg, char** buffer) {

    int status;
    DataBuffer db = {0};
    uint16_t msg_len = 0;
    
    switch (msg->type) {
    case MSG_PING: {

        // Get relevant data
        PingMessage* ping_msg = (PingMessage*)msg;
        uint32_t time = htonl(ping_msg->time);

        // Allocate memory for data buffer
        status = init_empty_buff(&db, sizeof(PingMessage));
        if (status == -1) return -1;

        // Increment databuffer pointer past header
        if (db_has_room(&db, 7)) inc_db(&db, 7);
        else {
            free(db.buffer);
            return -1;
        }

        // Serialize clock time
        if( (db_has_room(&db, sizeof(uint32_t)))) {
            memcpy(db.ptr, &time, sizeof(uint32_t));
            inc_db(&db, sizeof(uint32_t));
        }

        // Calculate number of bytes and trim buffer down to size
        msg_len = db.ptr - db.buffer;
        db.buffer = realloc(db.buffer, msg_len);
        break;
    }
    case MSG_USER_SETNAME:      // Intentional fall through
    case MSG_USER_CONNECT:      // Intentional fall through
    case MSG_USER_DISCONNECT: {

        // Get relevant data
        UserMessage* user_msg = (UserMessage*)msg;
        uint16_t id = htons(user_msg->id);
        int un_len;

        // Allocate data buffer memory
        status = init_empty_buff(&db, sizeof(UserMessage));
        if (status == -1) return -1;

        // Increment databuffer pointer past header
        if (db_has_room(&db, 7)) inc_db(&db, 7);
        else {
            free(db.buffer);
            return -1;
        }

        // Serialize id
        if (db_has_room(&db, sizeof(uint16_t))) {
            memcpy(db.ptr, &id, sizeof(uint16_t));
            inc_db(&db, sizeof(uint16_t));
        } else {
            free(db.buffer);
            return -1;
        }
        
        // Get length of username, ensure it ends in a null byte
        un_len = strnlen(user_msg->username, MAX_USERNAME_LEN) + 1;
        if (user_msg->username[un_len - 1] != 0) {
            free(db.buffer);
            return -1;
        }

        // Serialize username
        if (db_has_room(&db, un_len)) {
            strncat(db.ptr, user_msg->username, MAX_USERNAME_LEN);
            inc_db(&db, un_len);  // Remember null byte
        } else {
            free(db.buffer);
            return -1;
        }

        // Calculate number of bytes and trim buffer down to size
        msg_len = db.ptr - db.buffer;
        db.buffer = realloc(db.buffer, msg_len);

        break;
    }
    case MSG_ACTIVE_USERS: {

        // Get relevant data
        ActiveUserMessage* user_msg = (ActiveUserMessage*)msg;
        uint8_t num_users = user_msg->num_users;
        uint16_t id;
        int un_len;

        // Allocate data buffer memory
        status = init_empty_buff(&db, sizeof(ActiveUserMessage));
        if (status == -1) return -1;

        // Increment databuffer pointer past header
        if (db_has_room(&db, 7)) inc_db(&db, 7);
        else {
            free(db.buffer);
            return -1;
        }

        // Serialize num_users
        if (db_has_room(&db, sizeof(uint8_t))) {
            memcpy(db.ptr, &num_users, sizeof(uint8_t));
            inc_db(&db, sizeof(uint8_t));
        } else {
            free(db.buffer);
            return -1;
        }

        // Now serialize array of user ids
        for (int i = 0; i < num_users; i++) {
            if (db_has_room(&db, sizeof(uint16_t))) {
                id = htons(user_msg->ids[i]);
                memcpy(db.ptr, &id, sizeof(uint16_t));
                inc_db(&db, sizeof(uint16_t));
            } else {
                free(db.buffer);
                return -1;
            }
        }

        // And serialize array of user names, upto max username length
        // This will truncate strings that overwrite the null byte
        for (int i = 0; i < num_users; i++) {

            // Get length of username, ensure it ends in a null byte
            un_len = strnlen(user_msg->usernames[i], MAX_USERNAME_LEN) + 1;
            if (user_msg->usernames[i][un_len - 1] != 0) {
                free(db.buffer);
                return -1;
            }

            // Serialize username
            if (db_has_room(&db, un_len)) {
                strncat(db.ptr, user_msg->usernames[i], MAX_USERNAME_LEN);
                inc_db(&db, un_len);
            } else {
                free(db.buffer);
                return -1;
            }
        }
        
        // Calculate number of bytes and trim buffer down to size
        msg_len = db.ptr - db.buffer;
        db.buffer = realloc(db.buffer, msg_len);

        break;
    }
    case MSG_CHAT: {
        
        int chat_len;

        // Get relevant data
        ChatMessage* chat_msg = (ChatMessage*)msg;

        // Allocate data buffer memory
        status = init_empty_buff(&db, sizeof(ChatMessage));
        if (status == -1) return -1;

        // Increment databuffer pointer past header
        if (db_has_room(&db, 7)) inc_db(&db, 7);
        else {
            free(db.buffer);
            return -1;
        }

        // Get length of chat string, ensure it ends in a null byte
        chat_len = strnlen(chat_msg->msg, MAX_CHATMSG_LEN) + 1;
        if (chat_msg->msg[chat_len - 1] != 0) {
            free(db.buffer);
            return -1;
        }

        // Serialize message
        if (db_has_room(&db, chat_len)) {
            strncat(db.ptr, chat_msg->msg, MAX_CHATMSG_LEN);
            inc_db(&db, chat_len);
        } else {
            free(db.buffer);
            return -1;
        }
        
        // Calculate number of bytes and trim buffer down to size
        msg_len = db.ptr - db.buffer;
        db.buffer = realloc(db.buffer, msg_len);

        break;
    }
    case MSG_ERROR: {

        int err_len;
        
        // Get relevant data
        ErrorMessage* err_msg = (ErrorMessage*)msg;

        // Allocate data buffer memory
        status = init_empty_buff(&db, sizeof(ErrorMessage));
        if (status == -1) return -1;

        // Increment databuffer pointer past header
        if (db_has_room(&db, 7)) inc_db(&db, 7);
        else {
            free(db.buffer);
            return -1;
        }

        // Get length of chat string, ensure it ends in a null byte
        err_len = strnlen(err_msg->msg, MAX_CHATMSG_LEN) + 1;
        if (err_msg->msg[err_len - 1] != 0) {
            free(db.buffer);
            return -1;
        }

        // Serialize message
        if (db_has_room(&db, err_len)) {
            strncat(db.ptr, err_msg->msg, MAX_CHATMSG_LEN);
            inc_db(&db, err_len);
        } else {
            free(db.buffer);
            return -1;
        }
        
        // Calculate number of bytes and trim buffer down to size
        msg_len = db.ptr - db.buffer;
        db.buffer = realloc(db.buffer, msg_len);

        break;
    }
    default:
        return -1;
    }

    // Now fill in header information
    uint16_t nw_len = htons(msg_len);       // Message Length
    uint16_t nw_from = htons(msg->from);    // From
    uint16_t nw_to = htons(msg->to);        // To

    db.buffer[0] = (uint8_t)msg->type;        // Type
    memcpy(&(db.buffer[1]), &nw_len, 2);    // Length
    memcpy(&(db.buffer[3]), &nw_from, 2);   // From
    memcpy(&(db.buffer[5]), &nw_to, 2);     // To

    // Set return value
    *buffer = db.buffer;

    return msg_len;
}

// Deserialize a message, function will malloc required memory and store in message pointer
// Return pointer to deserialized message. Ownership passess to caller.
MessageHeader* deserialize_msg(char* buffer, int buffer_size) {

    // Initialize our data buffer structure
    DataBuffer db = {0};
    db.buffer = buffer;
    db.ptr = buffer;
    db.size = buffer_size;

    int un_len;

    // Allocate memory for a message header
    MessageHeader* msg = calloc(1, sizeof(MessageHeader));
    if (msg == NULL) return NULL;

    // Deserialize the header
    if (db_has_room(&db, 1)) {
        msg->type = db.ptr[0];  // Type
        inc_db(&db, 1);
    } else {
        free(msg);
        return NULL;
    }

    // Deserialize Message Length
    if (db_has_room(&db, 2)) {
        memcpy(&msg->len, db.ptr, 2); 
        msg->len = (uint16_t)ntohs(msg->len);   // Ensure data is host-endian
        inc_db(&db, 2);
    } else {
        free(msg);
        return NULL;
    }

    // Double check that our buffer size is the same as the message length
    if (db.size != msg->len) {
        free(msg);
        return NULL;
    }

    // Deserialize Message From
    if (db_has_room(&db, 2)) {
        memcpy(&msg->from, db.ptr, 2); 
        msg->from = (uint16_t)ntohs(msg->from);   // Ensure data is host-endian
        inc_db(&db, 2);
    } else {
        free(msg);
        return NULL;
    }

    // Deserialize Message To
    if (db_has_room(&db, 2)) {
        memcpy(&msg->to, db.ptr, 2); 
        msg->to = (uint16_t)ntohs(msg->to);   // Ensure data is host-endian
        inc_db(&db, 2);
    } else {
        free(msg);
        return NULL;
    }

    switch (msg->type) {
    case MSG_PING: {

        // Reallocate enough room for this message, set to all zeroes
        PingMessage* ping_msg = (PingMessage*)realloc(msg, sizeof(PingMessage));
        if (ping_msg == NULL) {
            free(msg);
            return NULL;
        }
        memset((char*)ping_msg + sizeof(MessageHeader), 0, sizeof(PingMessage) - sizeof(MessageHeader));

        // Deserialize time
        if (db_has_room(&db, sizeof(uint32_t))) {
            memcpy(&ping_msg->time, db.ptr, sizeof(uint32_t)); 
            ping_msg->time = (uint32_t)ntohl(ping_msg->time);   // Ensure data is host-endian
            inc_db(&db, sizeof(uint32_t));
        }

        // Confirm we reached the end of the buffer
        if (db_has_room(&db, 1)) {
            free(msg);
            return NULL;
        }

        return (MessageHeader*)ping_msg;
    }
    case MSG_USER_SETNAME:      // Intentional fall through
    case MSG_USER_CONNECT:      // Intentional fall through
    case MSG_USER_DISCONNECT: {

        // Reallocate enough room for this message, set to all zeroes
        UserMessage* user_msg = (UserMessage*)realloc(msg, sizeof(UserMessage));
        if (user_msg == NULL) {
            free(msg);
            return NULL;
        }
        memset((char*)user_msg + sizeof(MessageHeader), 0, sizeof(UserMessage) - sizeof(MessageHeader));

        // Deserialize id
        if (db_has_room(&db, 2)) {
            memcpy(&user_msg->id, db.ptr, 2); 
            user_msg->id = (uint16_t)ntohs(user_msg->id);   // Ensure data is host-endian
            inc_db(&db, 2);
        } else {
            free(user_msg);
            return NULL;
        }
        
        // Deserialize username
        un_len = db_strnlen(&db, MAX_USERNAME_LEN + 1);
        if (un_len != -1) {
            memcpy(&user_msg->username, db.ptr, un_len); 
            inc_db(&db, un_len);
        } else {
            free(user_msg);
            return NULL;
        }

        // Confirm we reached the end of the buffer
        if (db_has_room(&db, 1)) {
            free(user_msg);
            return NULL;
        }

        return (MessageHeader*)user_msg;
        
    }
    case MSG_ACTIVE_USERS: {

        // Reallocate enough room for this message, set to all zeroes
        ActiveUserMessage* user_msg = (ActiveUserMessage*)realloc(msg, sizeof(ActiveUserMessage));
        if (user_msg == NULL) {
            free(msg);
            return NULL;
        }
        memset((char*)user_msg + sizeof(MessageHeader), 0, sizeof(ActiveUserMessage) - sizeof(MessageHeader));

        // Deserialize number of users
        if (db_has_room(&db, 1)) { 
            user_msg->num_users = (uint8_t)db.ptr[0];
            inc_db(&db, 1);
        } else {
            free(user_msg);
            return NULL;
        }

        // Now deserialize array of user ids
        for (int i = 0; i < user_msg->num_users; i++) {
            if (db_has_room(&db, 2)) {
                memcpy(&(user_msg->ids[i]), db.ptr, 2); 
                user_msg->ids[i] = (uint16_t)ntohs(user_msg->ids[i]);   // Ensure data is host-endian
                inc_db(&db, 2);
            } else {
                free(user_msg);
                return NULL;
            }
        }

        // And serialize array of user names
        for (int i = 0; i < user_msg->num_users; i++) {
            un_len = db_strnlen(&db, MAX_USERNAME_LEN + 1);
            if (un_len != -1) {
                memcpy(&user_msg->usernames[i], db.ptr, un_len); 
                inc_db(&db, un_len);
            } else {
                free(user_msg);
                return NULL;
            }
        }

        // Confirm we reached the end of the buffer
        if (db_has_room(&db, 1)) {
            free(user_msg);
            return NULL;
        }

        return (MessageHeader*)user_msg;
    }
    case MSG_CHAT: {
    
        // Reallocate enough room for this message, set to all zeroes
        ChatMessage* chat_msg = (ChatMessage*)realloc(msg, sizeof(ChatMessage));
        if (chat_msg == NULL) {
            free(msg);
            return NULL;
        }
         memset((char*)chat_msg + sizeof(MessageHeader), 0, sizeof(ChatMessage) - sizeof(MessageHeader));
            
        // Deserialize string
        un_len = db_strnlen(&db, MAX_CHATMSG_LEN + 1);
        if (un_len != -1) {
            memcpy(&chat_msg->msg, db.ptr, un_len); 
            inc_db(&db, un_len);
        } else {
            free(chat_msg);
            return NULL;
        }

        // Confirm we reached the end of the buffer
        if (db_has_room(&db, 1)) {
            free(chat_msg);
            return NULL;
        }
    
        return (MessageHeader*)chat_msg;
    }
    case MSG_ERROR: {
    
        // Reallocate enough room for this message, set to all zeroes
        ErrorMessage* err_msg = (ErrorMessage*)realloc(msg, sizeof(ErrorMessage));
        if (err_msg == NULL) {
            free(msg);
            return NULL;
        }
         memset((char*)err_msg + sizeof(MessageHeader), 0, sizeof(ErrorMessage) - sizeof(MessageHeader));
            
        // Deserialize string
        un_len = db_strnlen(&db, MAX_CHATMSG_LEN + 1);
        if (un_len != -1) {
            memcpy(&err_msg->msg, db.ptr, un_len); 
            inc_db(&db, un_len);
        } else {
            free(err_msg);
            return NULL;
        }

        // Confirm we reached the end of the buffer
        if (db_has_room(&db, 1)) {
            free(err_msg);
            return NULL;
        }
    
        return (MessageHeader*)err_msg;
    }
    default:
        free(msg);
        return NULL;
    }
}
