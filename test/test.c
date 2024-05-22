#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "../src/sock.h"
#include "../src/chat.h"

void print_buffer(char* buffer, size_t num_bytes) {
    for (size_t i = 0; i < num_bytes; i++) {
        printf("%x ", (uint8_t)buffer[i]);
    }
    printf("\n");
}

bool serial_deserial_ping_msg_test(bool verbose) {

    // Create message
    PingMessage msg = {0};
    msg.header.type = MSG_PING;
    msg.header.from = 0;
    msg.header.to = 65535;

    msg.time = 100000;

    size_t num_bytes;
    char* buffer;

    num_bytes = serialize_msg((MessageHeader*)&msg, &buffer);

    msg.header.len = num_bytes; // Message length is calculated and packed by the serialization function

    PingMessage* out;

    out = (PingMessage*)deserialize_msg(buffer, num_bytes);

    if (out == NULL) return false;

    if (verbose) {
        printf("--------------------------------\n");
        print_buffer(buffer, num_bytes);
        printf("Message Len Before: %d After: %d\n", msg.header.len, out->header.len);
        printf("Message Type Before: %d After: %d\n", msg.header.type, out->header.type);
        printf("Message From Before: %d After: %d\n", msg.header.from, out->header.from);
        printf("Message To Before: %d After: %d\n", msg.header.to, out->header.to);
    }

    bool match = memcmp(&msg, out, sizeof(PingMessage)) == 0;

    free(buffer);
    free(out);

    return match;
}

bool serial_deserial_user_msg_test(bool verbose, char* username) {

    // Create message
    UserMessage msg = {0};
    msg.header.type = MSG_USER_SETNAME;
    msg.header.from = 10;
    msg.header.to = 0;
    msg.id = 10;
    strncpy(msg.username, username, MAX_USERNAME_LEN);

    size_t num_bytes;
    char* buffer;

    num_bytes = serialize_msg((MessageHeader*)&msg, &buffer);

    msg.header.len = num_bytes; // Message length is calculated and packed by the serialization function

    UserMessage* out;

    out = (UserMessage*)deserialize_msg(buffer, num_bytes);

    if (verbose) {
        printf("\n--------------------------------\n");
        print_buffer(buffer, num_bytes);
        printf("'Len' Before: %d After: %d\n", msg.header.len, out->header.len);
        printf("'Type' Before: %d After: %d\n", msg.header.type, out->header.type);
        printf("'From' Before: %d After: %d\n", msg.header.from, out->header.from);
        printf("'To' Before: %d After: %d\n", msg.header.to, out->header.to);
        printf("'id' Before: %d After: %d\n", msg.id, out->id);
        printf("'Username' Before: %s After: %s\n", msg.username, out->username);
    }

    bool match = memcmp(&msg, out, sizeof(UserMessage)) == 0;

    free(buffer);
    free(out);

    return match;
}

bool serial_deserial_active_msg_test(bool verbose, int num_users) {
    // Create message
    ActiveUserMessage msg = {0};
    msg.header.type = MSG_ACTIVE_USERS;
    msg.header.from = 777;
    msg.header.to = 80;

    msg.num_users = num_users;

    for (int i = 0; i < msg.num_users; i++) {
        msg.ids[i] = i + 10000;
        strncpy(msg.usernames[i],"Abcdefghijklmnopqrstuvwxyz",(i < MAX_USERNAME_LEN ? i : MAX_USERNAME_LEN));
    }

    size_t num_bytes;
    char* buffer;

    num_bytes = serialize_msg((MessageHeader*)&msg, &buffer);

    msg.header.len = num_bytes; // Message length is calculated and packed by the serialization function

    ActiveUserMessage* out;

    out = (ActiveUserMessage*)deserialize_msg(buffer, num_bytes);

    if (verbose) {
        printf("--------------------------------\n");
        print_buffer(buffer, num_bytes);
        printf("'Len' Before: %d After: %d\n", msg.header.len, out->header.len);
        printf("'Type' Before: %d After: %d\n", msg.header.type, out->header.type);
        printf("'From' Before: %d After: %d\n", msg.header.from, out->header.from);
        printf("'To' Before: %d After: %d\n", msg.header.to, out->header.to);
        printf("'num_users' Before: %d After: %d\n", msg.num_users, out->num_users);
        printf("First 'id' Before: %d After: %d\n", msg.ids[0], out->ids[0]);
        printf("First 'username' Before: %s After: %s\n", msg.usernames[0], out->usernames[0]);
        if (num_users > 1) {
            printf("Last 'id' Before: %d After: %d\n", msg.ids[num_users - 1], out->ids[num_users - 1]);
            printf("Last 'username' Before: %s After: %s\n", msg.usernames[num_users - 1], out->usernames[num_users - 1]);
        }
    }

    bool match = memcmp(&msg, out, sizeof(ActiveUserMessage)) == 0;

    free(buffer);
    free(out);

    return match;
}

bool serial_deserial_chat_msg_test(bool verbose, char* message) {

    // Create message
    ChatMessage msg = {0};
    msg.header.type = MSG_CHAT;
    msg.header.from = 999;
    msg.header.to = 23;
    strncpy(msg.msg, message, MAX_CHATMSG_LEN);

    size_t num_bytes;
    char* buffer;

    num_bytes = serialize_msg((MessageHeader*)&msg, &buffer);

    msg.header.len = num_bytes; // Message length is calculated and packed by the serialization function

    ChatMessage* out;

    out = (ChatMessage*)deserialize_msg(buffer, num_bytes);

    if (verbose) {
        printf("\n--------------------------------\n");
        print_buffer(buffer, num_bytes);
        printf("'Len' Before: %d After: %d\n", msg.header.len, out->header.len);
        printf("'Type' Before: %d After: %d\n", msg.header.type, out->header.type);
        printf("'From' Before: %d After: %d\n", msg.header.from, out->header.from);
        printf("'To' Before: %d After: %d\n", msg.header.to, out->header.to);
        printf("'Msg' Before: %s After: %s\n", msg.msg, out->msg);
    }

    bool match = memcmp(&msg, out, sizeof(ChatMessage)) == 0;

    free(buffer);
    free(out);

    return match;
}

bool serial_deserial_err_msg_test(bool verbose, char* message) {

    // Create message
    ErrorMessage msg = {0};
    msg.header.type = MSG_ERROR;
    msg.header.from = 0;
    msg.header.to = 1024;
    strncpy(msg.msg, message, MAX_CHATMSG_LEN);

    size_t num_bytes;
    char* buffer;

    num_bytes = serialize_msg((MessageHeader*)&msg, &buffer);

    msg.header.len = num_bytes; // Message length is calculated and packed by the serialization function

    ErrorMessage* out;

    out = (ErrorMessage*)deserialize_msg(buffer, num_bytes);

    if (verbose) {
        printf("\n--------------------------------\n");
        print_buffer(buffer, num_bytes);
        printf("'Len' Before: %d After: %d\n", msg.header.len, out->header.len);
        printf("'Type' Before: %d After: %d\n", msg.header.type, out->header.type);
        printf("'From' Before: %d After: %d\n", msg.header.from, out->header.from);
        printf("'To' Before: %d After: %d\n", msg.header.to, out->header.to);
        printf("'Msg' Before: %s After: %s\n", msg.msg, out->msg);
    }

    bool match = memcmp(&msg, out, sizeof(ErrorMessage)) == 0;

    free(buffer);
    free(out);

    return match;
}

bool corrupt_serial_user_msg_test(bool verbose) {

    (void) verbose; // Ignore compiler warnings

    // Create message
    UserMessage msg = {0};
    msg.header.type = MSG_USER_SETNAME;
    msg.header.from = 777;
    msg.header.to = 80;

    // Overwrite all data except message type
    memset((char*)(&msg) + 1,'a',sizeof(UserMessage) - 1);

    int num_bytes;
    char* buffer;

    num_bytes = serialize_msg((MessageHeader*)&msg, &buffer);

    if (num_bytes == -1) return true;

    return false;

}

bool corrupt_serial_active_user_msg_test(bool verbose) {

    (void) verbose; // Ignore compiler warnings

    // Create message
    ActiveUserMessage msg = {0};
    msg.header.type = MSG_ACTIVE_USERS;
    msg.header.from = 777;
    msg.header.to = 80;

    // Overwrite all data except message type
    memset((char*)(&msg) + 1,'a',sizeof(ActiveUserMessage) - 1);

    int num_bytes;
    char* buffer;

    num_bytes = serialize_msg((MessageHeader*)&msg, &buffer);

    if (num_bytes == -1) return true;

    return false;

}


bool corrupt_serial_chat_msg_test(bool verbose) {

    (void) verbose; // Ignore compiler warnings

    // Create message
    ChatMessage msg = {0};
    msg.header.type = MSG_CHAT;
    msg.header.from = 999;
    msg.header.to = 23;

    // Overwrite null byte in string to confirm we don't overflow
    memset(msg.msg,'a',MAX_CHATMSG_LEN + 1);

    int num_bytes;
    char* buffer;

    num_bytes = serialize_msg((MessageHeader*)&msg, &buffer);

    if (num_bytes == -1) return true;

    return false;

}

bool corrupt_serial_err_msg_test(bool verbose) {

    (void) verbose; // Ignore compiler warnings

    // Create message
    ErrorMessage msg = {0};
    msg.header.type = MSG_ERROR;
    msg.header.from = 728;
    msg.header.to = 37;

    // Overwrite null byte in string to confirm we don't overflow
    memset(msg.msg,'a',MAX_CHATMSG_LEN + 1);

    int num_bytes;
    char* buffer;

    num_bytes = serialize_msg((MessageHeader*)&msg, &buffer);

    if (num_bytes == -1) return true;

    return false;

}

bool corrupt_serial_invalid_type_test(bool verbose) {

    (void) verbose; // Ignore compiler warnings

    // Create message
    PingMessage msg = {0};
    msg.header.type = 99;
    msg.header.from = 56;
    msg.header.to = 12;

    int num_bytes;
    char* buffer;

    num_bytes = serialize_msg((MessageHeader*)&msg, &buffer);

    if (num_bytes == -1) return true;

    return false;

}

bool corrupt_deserial_user_msg_test(bool verbose) {

    (void) verbose; // Ignore compiler warnings

    // Create message
    UserMessage msg = {0};
    msg.header.type = MSG_USER_SETNAME;
    msg.header.from = 10;
    msg.header.to = 0;
    msg.id = 10;
    strncpy(msg.username, "Username12345678", MAX_USERNAME_LEN);

    size_t num_bytes;
    char* buffer;

    num_bytes = serialize_msg((MessageHeader*)&msg, &buffer);

    msg.header.len = num_bytes; // Message length is calculated and packed by the serialization function

    // Overwrite null byte of username
    buffer[num_bytes - 1] = 'a';

    UserMessage* out;

    out = (UserMessage*)deserialize_msg(buffer, num_bytes);

    free(buffer);

    // Confirm deserialization fails gracefully
    if (out == NULL) {
        return true;
    } else {
        free(out);
        return false;
    }

}

bool corrupt_deserial_active_user_msg_test(bool verbose) {

    (void) verbose; // Ignore compiler warnings

    // Create message
    ActiveUserMessage msg = {0};
    msg.header.type = MSG_ACTIVE_USERS;
    msg.header.from = 777;
    msg.header.to = 80;

    msg.num_users = 10;

    for (int i = 0; i < msg.num_users; i++) {
        msg.ids[i] = i + 10000;
        strncpy(msg.usernames[i],"Abcdefghijklmnopqrstuvwxyz",(i < MAX_USERNAME_LEN ? i : MAX_USERNAME_LEN));
    }

    size_t num_bytes;
    char* buffer;

    num_bytes = serialize_msg((MessageHeader*)&msg, &buffer);

    msg.header.len = num_bytes; // Message length is calculated and packed by the serialization function

    // Overwrite last null byte
    buffer[num_bytes - 1] = 'a';

    ActiveUserMessage* out;

    out = (ActiveUserMessage*)deserialize_msg(buffer, num_bytes);

    free(buffer);

    // Confirm deserialization fails gracefully
    if (out == NULL) {
        return true;
    } else {
        free(out);
        return false;
    }
}

bool corrupt_deserial_chat_msg_test(bool verbose) {

    (void) verbose; // Ignore compiler warnings

    // Create message
    ChatMessage msg = {0};
    msg.header.type = MSG_CHAT;
    msg.header.from = 999;
    msg.header.to = 23;
    strncpy(msg.msg, "Testing 123", MAX_CHATMSG_LEN);

    size_t num_bytes;
    char* buffer;

    num_bytes = serialize_msg((MessageHeader*)&msg, &buffer);

    msg.header.len = num_bytes; // Message length is calculated and packed by the serialization function

    // Overwrite last null byte
    buffer[num_bytes - 1] = 'a';

    ChatMessage* out;

    out = (ChatMessage*)deserialize_msg(buffer, num_bytes);

    // Confirm deserialization fails gracefully
    if (out == NULL) {
        return true;
    } else {
        free(out);
        return false;
    }
}

bool corrupt_deserial_err_msg_test(bool verbose) {

    (void) verbose; // Ignore compiler warnings

    // Create message
    ErrorMessage msg = {0};
    msg.header.type = MSG_ERROR;
    msg.header.from = 656;
    msg.header.to = 3;
    strncpy(msg.msg, "Testing 123", MAX_CHATMSG_LEN);

    size_t num_bytes;
    char* buffer;

    num_bytes = serialize_msg((MessageHeader*)&msg, &buffer);

    msg.header.len = num_bytes; // Message length is calculated and packed by the serialization function

    // Overwrite last null byte
    buffer[num_bytes - 1] = 'a';

    ErrorMessage* out;

    out = (ErrorMessage*)deserialize_msg(buffer, num_bytes);

    // Confirm deserialization fails gracefully
    if (out == NULL) {
        return true;
    } else {
        free(out);
        return false;
    }
}

bool corrupt_deserial_invalid_type_test(bool verbose) {

    (void) verbose; // Ignore compiler warnings

    // Create message
    PingMessage msg = {0};
    msg.header.type = MSG_PING;
    msg.header.from = 656;
    msg.header.to = 3;

    size_t num_bytes;
    char* buffer;

    num_bytes = serialize_msg((MessageHeader*)&msg, &buffer);

    msg.header.len = num_bytes; // Message length is calculated and packed by the serialization function

    // Overwrite type byte
    buffer[0] = (uint8_t)99;

    ErrorMessage* out;

    out = (ErrorMessage*)deserialize_msg(buffer, num_bytes);

    // Confirm deserialization fails gracefully
    if (out == NULL) {
        return true;
    } else {
        free(out);
        return false;
    }
}



int main(int argc, char* argv[]) {

    int verbose = false;


    if (argc == 2) {
        if (strcmp("-v",argv[1]) == 0) {
            verbose = true;
        } else if (strcmp("-h",argv[1]) == 0) {
            printf("run_test [-v] - Run Serial/Deserial Test Cases");
        }else {
            printf("Invalid option. See -h for help.");
        }
        
    } else if (argc > 2) {
        printf("Invalid number of arguments. See -h for help.");
    }

    printf("PingMessage Test 1: %s\n", serial_deserial_ping_msg_test(verbose) ? "PASS" : "FAIL");
    printf("UserMessage Test 1: %s\n", serial_deserial_user_msg_test(verbose, "Alex") ? "PASS" : "FAIL");
    printf("UserMessage Test 2: %s\n", serial_deserial_user_msg_test(verbose, "") ? "PASS" : "FAIL");
    printf("UserMessage Test 3: %s\n", serial_deserial_user_msg_test(verbose, "AReallyLongNameLikeThis") ? "PASS" : "FAIL");
    printf("ActiveUserMessage Test 1: %s\n", serial_deserial_active_msg_test(verbose, 0) ? "PASS" : "FAIL");
    printf("ActiveUserMessage Test 2: %s\n", serial_deserial_active_msg_test(verbose, 10) ? "PASS" : "FAIL");
    printf("ActiveUserMessage Test 3: %s\n", serial_deserial_active_msg_test(verbose, MAX_CLIENTS) ? "PASS" : "FAIL");
    printf("ChatMessage Test 1: %s\n", serial_deserial_chat_msg_test(verbose, "Test Message 1") ? "PASS" : "FAIL");
    printf("ChatMessage Test 2: %s\n", serial_deserial_chat_msg_test(verbose, "") ? "PASS" : "FAIL");
    printf("ChatMessage Test 3: %s\n", serial_deserial_chat_msg_test(verbose, "A very long message that exceeds the maximum message length. This message needs to exceed the 256 character limit, so it will go on and on and on and on and on and on and on and on. Its still not quite long enough though, so it will keep going on and on and on.") ? "PASS" : "FAIL");
    printf("ErrorMessage Test 1: %s\n", serial_deserial_err_msg_test(verbose, "Test Message 1") ? "PASS" : "FAIL");
    printf("ErrorMessage Test 2: %s\n", serial_deserial_err_msg_test(verbose, "") ? "PASS" : "FAIL");
    printf("ErrorMessage Test 3: %s\n", serial_deserial_err_msg_test(verbose, "A very long message that exceeds the maximum message length. This message needs to exceed the 256 character limit, so it will go on and on and on and on and on and on and on and on. Its still not quite long enough though, so it will keep going on and on and on.") ? "PASS" : "FAIL");
    printf("Corrupt Message Serialization 1: %s\n", corrupt_serial_user_msg_test(verbose) ? "PASS" : "FAIL");  
    printf("Corrupt Message Serialization 2: %s\n", corrupt_serial_active_user_msg_test(verbose) ? "PASS" : "FAIL");    
    printf("Corrupt Message Serialization 3: %s\n", corrupt_serial_chat_msg_test(verbose) ? "PASS" : "FAIL");
    printf("Corrupt Message Serialization 4: %s\n", corrupt_serial_err_msg_test(verbose) ? "PASS" : "FAIL");
    printf("Corrupt Message Serialization 5: %s\n", corrupt_serial_invalid_type_test(verbose) ? "PASS" : "FAIL");
    printf("Corrupt Message Deserialization 1: %s\n", corrupt_deserial_user_msg_test(verbose) ? "PASS" : "FAIL");  
    printf("Corrupt Message Deserialization 2: %s\n", corrupt_deserial_active_user_msg_test(verbose) ? "PASS" : "FAIL");    
    printf("Corrupt Message Deserialization 3: %s\n", corrupt_deserial_chat_msg_test(verbose) ? "PASS" : "FAIL");
    printf("Corrupt Message Deserialization 4: %s\n", corrupt_deserial_err_msg_test(verbose) ? "PASS" : "FAIL");
    printf("Corrupt Message Deserialization 5: %s\n", corrupt_deserial_invalid_type_test(verbose) ? "PASS" : "FAIL");

}
