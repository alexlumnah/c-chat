#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include "chat.h"    

// Print help info
void print_help(void) {
    printf("usage: chat [-h] [-s] [-u <server host>] <port_number>\n");
    printf("\t-h:\t\t\tPrint help message.\n");
    printf("\t-s:\t\t\tStart server. (Defaults to client).\n");
    printf("\t-u <server_host>:\tConnect to specified host. Defaults to localhost.\n");
    printf("\t<port_number>: \t\tPort number to connect to.\n");
}

int main(int argc, char* argv[]) {

    char c;

    // Default client/server options
    bool chat_server = false;
    const char* host = "localhost";
    const char* port = NULL;

    ChatStatus status;

    // Parse input options
    while ((c = getopt(argc, argv, ":hsu:")) != -1) {
        switch (c) {
        case 'h':
            print_help();
            return 0;
        case 's':
            chat_server = true;
            break;
        case 'u':
            host = optarg;
            break;
        case ':':
            printf("Option '-%c' needs argument.\n", optopt);
            print_help();
            return -1;
        default:
            print_help();
            return -1;
        }
    }

    // Now parse port positional argument
    if (argv[optind] == NULL) {
        printf("[ERROR] Missing positional argument <port_number>.\n");
        print_help();
        return -1;
    }
    port = argv[optind];

    if (chat_server) {

        // Initialize chat server
        status = start_chat_server(port);

        if (status != CHAT_FAILURE) {
            chat_server_run();
        }

        return -1;
    } else {

        // Initialize chat client
        printf("Connecting to chat server...\n");
        status = start_chat_client(host, port);

        if (status == CHAT_FAILURE) {
            printf("Connection failed. Exiting\n");
            return -1;
        }

        // Run client until exit
        status = client_run();

        // Shutdown client
        end_chat_client();

        if (status == CHAT_FAILURE) {
            printf("Server disconnected unexpectedly.\n");
            return -1;
        }

        return 0;
    }
    
}


