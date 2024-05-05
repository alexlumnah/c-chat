#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "sock.h"

int main(void) {

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
}
