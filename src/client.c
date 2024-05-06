#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "sock.h"
#include "chat.h"



int main(void) {

    ActiveUserMessage msg = {0};
    msg.header.type = MSG_ACTIVE_USERS;
    msg.header.from = 10;
    msg.header.to = 25;

    msg.num_users = MAX_CLIENTS;

    for (int i = 0; i < msg.num_users; i++) {
        msg.ids[i] = i + 10000;
        strncpy(msg.usernames[i],"AlexAlexAlexAlex123",MAX_USERNAME_LEN + 1);
    }
    

    char* buffer;
    int num_bytes;
    MessageHeader* out;

    num_bytes = serialize_msg((MessageHeader*)&msg, &buffer);
    printf("Buffer length: %d\n", num_bytes);
    // Print serialized data
    printf("Serialized data: ");
    for (int i = 0; i < num_bytes; i++) {
        printf("%x ", buffer[i]);
    }
    printf("\n");


    deserialize_msg(buffer, num_bytes, &out);
    ActiveUserMessage* deserial = (ActiveUserMessage*)out;

    printf("Header: %d, %d\n",deserial->header.type, deserial->header.len);
    printf("From: %d, To: %d\n",deserial->header.from, deserial->header.to);
    printf("Num Users: %d\n",deserial->num_users);
    for (int i = 0; i < deserial->num_users; i++) {
        printf("%d: %s\n", deserial->ids[i], deserial->usernames[i]);
    }

/*
    int status;
    char buffer[MAX_MESSAGE_LEN];
    Message* msg;
    Message* prev_msg;

    status = start_client("localhost","7778");

    do {

        status = poll_sockets(1000);

        if (status != SOCK_SUCCESS) break;

        while ((msg = pop_msg()) != NULL) {
            printf("Received message from %d of length %d: %s\n",msg->sender, msg->len, msg->data);

            prev_msg = msg;
            msg = msg->next_msg;

            free(prev_msg);
        }

        if (buffer[0] != '\n') {
            printf("Sending message of length: %lu\n", strlen(buffer) + 1);
            client_send_msg(buffer, strlen(buffer) + 1);
        }

    } while (fgets(buffer, MAX_MESSAGE_LEN, stdin));

    printf("Status: %d\n",status);

    return 0;
    */
}
