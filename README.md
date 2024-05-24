# Chat
Simple chat application to learn the basics of networking programming via sockets.

## Files
- chat.c - Entrypoint into application. Start client or server.
- chat.h - Header for common types and public functions from client, server, serialization, and ui files.
- client.c - Source containing core chat client functionality.
- server.c - Source containing core chat server functionality.
- ui.c - Curses wrapper to create a basic terminal UI for chat client.
- serial.c - Serialization/Deserialization library.
- sock.c - Simple library that abstracts socket input/output for both client and server.

## Usage
    > ./chat -h
    usage: chat [-h] [-s] [-u <server host>] <port_number>
        -h:                 Print help message.
        -s:                 Start server.
        -u <server_host>:   Connect to specified host. Defaults to localhost.
        <port_number>:      Port number to connect to.

Start server on local host at port 7777:

    > ./chat -s 7777

Connect client to localhost at port 7777:

    > ./chat -u localhost 7777

## Future Improvements
This is a simple proof of concept demonstrating the basics of socket networking. Possible future improvements include:
* Encrypt data sent between client and server
* Confirm integrity and receipt of data, and resend any dropped/corrupted packets
* Improve UI to allow scrolling through previous messages
* Add capability for direct messaging between users