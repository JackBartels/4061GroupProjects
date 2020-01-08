/* CSci4061 F2018 Assignment 2
* section: 3
* date: 11/10/2018
* name: Lee Wintervold, Jack Bartels, Joe Keene
* Lee SID: 4123521 
* Jack SID: 5006022
* Joe SID: 3894741
*/

This program implements a pipe-based multiprocess chat server that users can connect to.

Lee - I worked on the server implementation and server command line handling.
joe - I worked on the client program
Jack - I worked on user command implementation and handling

To compile, run `make`.

Starting the server: `./server`

Starting a client: `./client <username>`

Server commands:
`\list`: list the names of all connected users
`\kick <username>`: kick the specified user from the server
`\exit`: close the server
`<any-other-text>`: broadcasts message to all users with prefix "admin"

User commands:
`\list`: list the names of all connected users
`\exit`: disconnects the user from the server
`\p2p <username> <message>`: send a direct message to <username>
`<any-other-text>`: broadcasts message to all users
`\seg`: (extra credit) create segmentation fault in user program 

What the program does:
The server polls for new user connections, and attempts to add that user if possible.
If the user is added, a child monitor process is spawned which facilitates communication
between the server and the user. The child monitor polls reading from the server output
and reading from the user output, writing messages to the user and server, respectively.
The server also polls for commands from stdin and messages/commands from a user (facilitated
by the child monitor for that user).

The user attempts to connect to a server if it exists. If it does, it polls reading from the
server (the child monitor process) for messages.

Assumptions:
- Usernames do not have spaces
- Messages are short enough to include all passed prefixes ("Notice: ", "<username>: ", etc)
- Messages do not have more than 16 tokens separated by spaces
- Messages are comprised of ASCII characters

Error Handling:
If the server cannot comminucate with a user, that user will be kicked. If a user cannot 
communicate with the server, that user exits. If a child monitor process cannot communicate
with either the server or its user, it exits.
