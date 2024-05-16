#include <stdio.h>
#include <string.h>
#include <curses.h>

#include "chat.h"    

int main(int argc, char* argv[]) {

    int status;

    // Server
    if (argc == 2 && strncmp(argv[1],"-s",2) == 0) {
        start_chat_server("7777");
        chat_server_run();
    } else {

        // Initialize chat client
        printf("Connecting to chat server...\n");
        status = start_chat_client("localhost","7777");

        if (status == -1) {
            printf("Connection failed. Exiting");
            return -1;
        }

        // Run client until exit
        client_run();

        // Shutdown client
        printf("Disconnecting from server.\n");
        end_chat_client();

    }
    
}


