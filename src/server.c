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

    while (1) {

        status = poll_sockets(1000);

        while ((msg = pop_msg()) != NULL) {

            // Echo message back
            server_send_msg(msg->sender, msg->data, msg->len);

            prev_msg = msg;
            msg = msg->next_msg;

            free(prev_msg);
        }
    }

    printf("Status: %d", status);

    return 0;
}
