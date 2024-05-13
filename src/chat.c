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

        char buffer[MAX_MESSAGE_LEN] = {0};
        int buff_len = 0;

        // Initialize chat client
        printf("Connecting to chat server...\n");
        status = start_chat_client("localhost","7777");
        ChatClient* client = get_client();

        printf("%d\n",status);

        // Initialize ncurses library
        initscr();
        keypad(stdscr, TRUE);
        noecho();
        timeout(0); // Don't block waiting for character
        scrollok(stdscr,TRUE);

        // Loop, check for messages, and print out new messages
        int c = 0;
        while (status == 1 && c != 27) {


            // Poll for inputs
            status = client_check_messages(1000);

            printf("%d\n",status);

            c = getch();

            if ((c == KEY_BACKSPACE || c == 127 || c == '\b') && buff_len > 0) {
                buff_len--;
                buffer[buff_len] = 0;
            } else if (c >= ' ' && c <= '~' && buff_len < MAX_MESSAGE_LEN - 1) {
                buffer[buff_len] = c;
                buff_len++;
            } else if (c == KEY_ENTER || c == '\n') {
                client_send_chat(SERVER_ID, buffer);
                memset(buffer, 0, buff_len);
                buff_len = 0;
            }

            // Draw to the screen
            // Naively print all received messages
            clear();
            move(0,0);
            for (int i = 0; i < client->num_msgs; i++) {
                wprintw(stdscr, "%d: %s\n", client->msgs[i].header.from, client->msgs[i].msg);
            }
            // Print buffer
            wprintw(stdscr, "> %s", buffer);
        }
        endwin();
        printf("Disconnecting from server.\n");
        shutdown_client();



    }

    
}
        /*
        // Throwaway first data
        fgets(buffer, MAX_MESSAGE_LEN, stdin);
        do {

            // Check for any messages
            client_check_messages(1000);



            if (buffer[0] =='\n') {

            } else if (strncmp(buffer, "setname ", 7) == 0) {
                client_req_user_setname(&buffer[8]);
            } else if (strncmp(buffer, "ping\n", 5) == 0) {
                client_ping_server();
            } else if (strncmp(buffer, "getusers\n", 9) == 0) {
                client_req_active_users();
            } else if (buffer[0] == '>') {
                client_req_active_users();
            } else {
                client_send_chat(0, buffer);
            }

        } while (fgets(buffer, MAX_MESSAGE_LEN, stdin));
        */

