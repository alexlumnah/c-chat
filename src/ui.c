#include <curses.h>
#include <stdarg.h>

#include "chat.h"

#define MAX_CHAT_HISTORY (500)
#define USER_WIDTH (23)
#define INPUT_HEIGHT (3)

WINDOW* chat;
WINDOW* input;
WINDOW* users;

void init_window(void) {

    // Initialize curses library
    initscr();
    noecho();
    keypad(stdscr, TRUE);
    timeout(0); // Dont wait block when waiting for input

    // Create windows for displaying text
    chat = newpad(MAX_CHAT_HISTORY, COLS - USER_WIDTH);
    input = newpad(INPUT_HEIGHT, COLS);
    users = newpad(255, USER_WIDTH);

    // Additional config
    scrollok(chat, TRUE);
    wsetscrreg(chat, 0, LINES - 3);

    // Initialize all screens
    refresh();
    //prefresh(chat, 0, 0, 0, 0, LINES - INPUT_HEIGHT, COLS - USER_WIDTH);
    //prefresh(users, 0, 0, 0, COLS - USER_WIDTH, LINES - INPUT_HEIGHT, COLS);
    //prefresh(input, 0, 0, LINES - INPUT_HEIGHT, 0, LINES, COLS);

}

void kill_window(void) {
    endwin();
}

void update_user_display(const User* user_list, int num_users) {

    int row = 0;
    wclear(users);
    wmove(users, 0, 0);
    wvline(users, '|', LINES - 3);

    wmove(users, row, 1);
    row++;
    wprintw(users, "Connected Users:");

    for (int i = 0; i < num_users; i++) {
        if (user_list[i].active != USER_ACTIVE)
            continue;

        wmove(users, row, 1);
        row++;
        wprintw(users, "%d: %s\n", user_list[i].id, user_list[i].name);
    }

    prefresh(users, 0, 0, 0, COLS - USER_WIDTH, LINES - INPUT_HEIGHT, COLS);
    
}

void printf_message(const char* fmt, ...) {

    // Do not print if window hasn't been initialized
    if (chat == NULL) {
        return;
    }

    // Initialize variable arguments
    va_list vargs;
    va_start(vargs, fmt);

    // Print formatted string
    vw_printw(chat, fmt, vargs);
    wprintw(chat, "\n");

    // Clean up variable arguments
    va_end(vargs);

    prefresh(chat, 0, 0, 0, 0, LINES - INPUT_HEIGHT, COLS - USER_WIDTH);
}

void draw_buffer(const char* buffer) {

    // Clear input bar, write buffer
    wclear(input);
    wmove(input, 0, 0);
    whline(input, '-', COLS);
    wmove(input, 1, 0);
    wprintw(input, "> %s", buffer);

    // Draw buffer
    prefresh(input, 0, 0, LINES - INPUT_HEIGHT, 0, LINES, COLS);
}
