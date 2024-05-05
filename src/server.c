#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "sock.h"

int main(void) {

    int status;
    status = start_server("7778");
    Message *msg;
    Message* prev_msg;

    do {

        while ((msg = pop_msg()) != NULL) {

            printf("Received message from %d of length %d: %s\n",msg->sender, msg->len, msg->data);
            // Echo message back
            char string[] = "We hear you!";
            server_send_msg(msg->sender, string, strlen(string) + 1);

            prev_msg = msg;
            msg = msg->next_msg;

            free(prev_msg);
        }
    } while (poll_sockets(1000) == SOCK_SUCCESS);

    printf("Status: %d", status);

    return 0;
}
