#include <stdio.h>
#include <string.h>

#include "chat.h"    

int main(int argc, const char* argv[]) {

    ChatStatus status;

    // Server
    if (argc == 2 && strncmp(argv[1],"-s",2) == 0) {

        status = start_chat_server("7777");

        if (status != CHAT_FAILURE) {
            chat_server_run();
        }

        return -1;

    } else {

        // Initialize chat client
        printf("Connecting to chat server...\n");
        status = start_chat_client("localhost","7777");

        if (status == CHAT_FAILURE) {
            printf("Connection failed. Exiting");
            return -1;
        }

        // Run client until exit
        status = client_run();

        // Shutdown client
        end_chat_client();

        if (status == CHAT_FAILURE) {
            printf("Server disconnected unexpectedly.\n");
        }

    }
    
}


