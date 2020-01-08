#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "comm.h"
#include "util.h"

/* -----------Functions that implement server functionality -------------------------*/

/*
 * Close pipes if a setting up a connection to a user failed
 */
void close_pipe_set(int p1[], int p2[]){
	if(-1 == close(p1[0])){
		perror("Failed to close pipe");
		exit(1);
	} 
	if(-1 == close(p1[1])){
		perror("Failed to close pipe");
		exit(1);
	}
       	if(-1 == close(p2[0])){
		perror("Failed to close pipe");
		exit(1);
	}	
       	if(-1 == close(p2[1])){
		perror("Failed to close pipe");
		exit(1);
	}
}


/*
 * Returns the empty slot on success, or -1 on failure
 */

int find_empty_slot(USER * user_list) {
	// iterate through the user_list and check m_status to see if any slot is EMPTY
	// return the index of the empty slot
    int i = 0;
	for(i=0;i<MAX_USER;i++) {
    	if(user_list[i].m_status == SLOT_EMPTY) {
			return i;
		}
	}
	return -1;
}

/*
 * list the existing users on the server shell
 */
int list_users(int idx, USER * user_list)
{
	// iterate through the user list
	// if you find any slot which is not empty, print that m_user_id
	// if every slot is empty, print "<no users>""
	// If the function is called by the server (that is, idx is -1), then printf the list
	// If the function is called by the user, then send the list to the user using write() and passing m_fd_to_user
	// return 0 on success
	int i, flag = 0;
	char buf[MAX_MSG] = {}, *s = NULL;

	/* construct a list of user names */
	s = buf;
	strncpy(s, "---connected user list---\n", strlen("---connected user list---\n"));
	s += strlen("---connected user list---\n");
	for (i = 0; i < MAX_USER; i++) {
		if (user_list[i].m_status == SLOT_EMPTY)
			continue;
		flag = 1;
		strncpy(s, user_list[i].m_user_id, strlen(user_list[i].m_user_id));
		s = s + strlen(user_list[i].m_user_id);
		strncpy(s, "\n", 1);
		s++;
	}
	if (flag == 0) {
		strcpy(buf, "<no users>\n");
	} else {
		strncpy(s, "\0", 1);
	}

	if(idx < 0) {
		printf("%s", buf);
	} else {
		/* write to the given pipe fd */
		if (write(user_list[idx].m_fd_to_user, buf, strlen(buf) + 1) < 0)
			perror("writing to server shell");
	}

	return 0;
}

/*
 * add a new user
 */
int add_user(int idx, USER * user_list, int pid, char * user_id, int fd_SERVER_read_from_monitor, int fd_SERVER_write_to_monitor)
{
	// MUST CALL find_empty_slot BEFORE THIS TO GET VALID INDEX PARAMETER
	USER newuser;
	newuser.m_pid = pid;
	strncpy(newuser.m_user_id, user_id, MAX_USER_ID);
	newuser.m_fd_to_user = fd_SERVER_write_to_monitor;
	newuser.m_fd_to_server = fd_SERVER_read_from_monitor;
	newuser.m_status = SLOT_FULL;

	user_list[idx] = newuser;

	// populate the user_list structure with the arguments passed to this function
	// return the index of user added
	return idx;
}

/*
 * Kill a user
 */
void kill_user(int idx, USER * user_list) {
	// kill a user (specified by idx) by using the systemcall kill()
	// then call waitpid on the user
	int pid = user_list[idx].m_pid;
	int status;
	kill(pid, SIGKILL);
	waitpid(pid, &status, 0);
}

/*
 * Perform cleanup actions after the used has been killed
 */
void cleanup_user(int idx, USER * user_list)
{
	// m_pid should be set back to -1
	// m_user_id should be set to zero, using memset()
	// close all the fd
	// set the value of all fd back to -1
	// set the status back to empty
	user_list[idx].m_pid = -1;
	memset(user_list[idx].m_user_id, '\0', MAX_USER_ID);
	if(close(user_list[idx].m_fd_to_user) < 0){
		perror("Failed to close a pipe. Killing server.\n");
		exit(1);
	}
	if(close(user_list[idx].m_fd_to_server) < 0){
		perror("Failed to close a pipe. Killing server.\n");
		exit(1);
	}
	user_list[idx].m_fd_to_user = -1;
	user_list[idx].m_fd_to_server = -1;
	user_list[idx].m_status = SLOT_EMPTY;
}

/*
 * Kills the user and performs cleanup
 */
void kick_user(int idx, USER * user_list) {
	// should kill_user()
	// then perform cleanup_user()
	kill_user(idx, user_list);
	cleanup_user(idx, user_list);
}

/*
 * broadcast message to all users
 */
int broadcast_msg(USER * user_list, char *msg, char *sender)
{
	char sendbuf[MAX_MSG];
	int name_end_index = strlen(sender);
	strncpy(sendbuf, sender, name_end_index);
	sendbuf[name_end_index] = ':';
	strncpy(sendbuf + name_end_index + 1, msg, MAX_MSG - name_end_index - 1);

	ssize_t writeStat;

	//broadcast to users
	for(int i = 0; i < MAX_USER; i++){
		if (user_list[i].m_status == SLOT_FULL && strcmp(user_list[i].m_user_id, sender)){
			writeStat = write(user_list[i].m_fd_to_user, sendbuf, strlen(sendbuf));

			if(writeStat == -1){exit(1);}
		}
	}
	//iterate over the user_list and if a slot is full, and the user is not the sender itself,
	//then send the message to that user
	//return zero on success
	return 0;
}

/*
 * find user index for given user name
 */
int find_user_index(USER * user_list, char * user_id)
{
	// go over the  user list to return the index of the user which matches the argument user_id
	// return -1 if not found
	int i, user_idx = -1;

	if (user_id == NULL) {
		fprintf(stderr, "NULL name passed.\n");
		return user_idx;
	}
	for (i=0;i<MAX_USER;i++) {
		if (user_list[i].m_status == SLOT_EMPTY)
			continue;
		if (strcmp(user_list[i].m_user_id, user_id) == 0) {
			return i;
		}
	}

	return -1;
}

/*
 * given a command's input buffer, extract name
 */
int extract_name(char * buf, char * user_name)
{
	char inbuf[MAX_MSG];
   	char * tokens[MAX_TOKEN_COUNT];
    	strcpy(inbuf, buf);

    	int token_cnt = parse_line(inbuf, tokens, " ");

    	if(token_cnt >= 2) {
        	strcpy(user_name, tokens[1]);
        	return 0;
    	}

    	return -1;
}
/*
 * Get the 3rd token in a message
 */
int extract_text(char *buf, char * text)
{
	char inbuf[MAX_MSG];
	char * tokens[MAX_TOKEN_COUNT];

	strcpy(inbuf, buf);

	int token_cnt = parse_line(inbuf, tokens, " ");

	if(token_cnt >= 3) {
        	int i;

		for(i=2; i < MAX_TOKEN_COUNT; i++)
		{
			if(tokens[i] == NULL) {break;}
			else
			{
				strncat(text, tokens[i], strlen(tokens[i]));
				strncat(text, " ", 1);
			}
		}

        	return 0;
    	}

    	return -1;
}

/*
 * send personal message
 */
void send_p2p_msg(int idx, USER * user_list, char *buf)
{
	// get the target user by name using extract_name() function
	// find the user id using find_user_index()
	// if user not found, write back to the original user "User not found", using the write()function on pipes.
	// if the user is found then write the message that the user wants to send to that user.

	//Gets target user
	char targetUserID[MAX_USER_ID];
	int nameExtractStat = extract_name(buf, targetUserID);

	//Gets target user index
	int targetUserIdx = find_user_index(user_list, targetUserID);

	//Gets message to send to target
	char textToSend[MAX_MSG];
	memset(&textToSend, 0, sizeof(textToSend));
	int textExtractStat = extract_text(buf, textToSend);

	ssize_t writeStat;

	if(targetUserIdx == -1) //User not found in user_list
	{
		char errMsg[] = "User not found";
		writeStat = write(user_list[idx].m_fd_to_user, errMsg, sizeof(errMsg));

		if(writeStat == -1){perror("Write failed");exit(1);}
	}
	else if(nameExtractStat == -1 || textExtractStat == -1) //Invalid syntax
	{
		char errMsg[] = "Invalid P2P command. Syntax: \\p2p <username> <message>";
		writeStat = write(user_list[idx].m_fd_to_user, errMsg, sizeof(errMsg));

		if(writeStat == -1){perror("Write failed");exit(1);}
	}
	else
	{
		//Writes message to target user
		char sender[MAX_MSG] = "\n";

		strncat(sender, user_list[idx].m_user_id, sizeof(user_list[idx].m_user_id));
		strncat(sender, " says: ", 7);
		strncat(sender, textToSend, sizeof(textToSend));

		writeStat = write(user_list[targetUserIdx].m_fd_to_user, sender, sizeof(sender));

		if(writeStat == -1){perror("Write failed");exit(1);}
	}
}

/*
 * Populates the user list initially
 */
void init_user_list(USER * user_list) {
	//iterate over the MAX_USER
	//memset() all m_user_id to zero
	//set all fd to -1
	//set the status to be EMPTY
	int i=0;
	for(i=0;i<MAX_USER;i++) {
		user_list[i].m_pid = -1;
		memset(user_list[i].m_user_id, '\0', MAX_USER_ID);
		user_list[i].m_fd_to_user = -1;
		user_list[i].m_fd_to_server = -1;
		user_list[i].m_status = SLOT_EMPTY;
	}
}
/*
 * Start child process to monitor a user
 */
void initialize_monitor(
		int pipe_MONITOR_reading_from_user[],
		int pipe_MONITOR_writing_to_user[],
		int pipe_SERVER_reading_from_monitor[],
		int pipe_SERVER_writing_to_monitor[]){


	int fd_read_from_user = pipe_MONITOR_reading_from_user[0];
	int fd_write_to_user = pipe_MONITOR_writing_to_user[1];
	int fd_read_from_server = pipe_SERVER_writing_to_monitor[0];
	int fd_write_to_server = pipe_SERVER_reading_from_monitor[1];
	fcntl(fd_read_from_user, F_SETFL, fcntl(fd_read_from_user, F_GETFL) | O_NONBLOCK);
	//close all pipes that won't be used
	if(-1 == close(pipe_MONITOR_reading_from_user[1]) ||
	   -1 == close(pipe_MONITOR_writing_to_user[0])   ||
	   -1 == close(pipe_SERVER_writing_to_monitor[1]) ||
	   -1 == close(pipe_SERVER_reading_from_monitor[0])){
		printf("Failed to close a pipe.\n");
		exit(1);
	}
	while(1){

		// read from server, send to user
		// If server->monitor pipe's write end is closed, exit
		int servreadstatus;
		int userwritestatus;
		char servrbuf[MAX_MSG];
		servreadstatus = read(fd_read_from_server, servrbuf, sizeof(servrbuf));
		if(servreadstatus == -1 && errno != EAGAIN){
			perror("Child process failed to read from server. Terminating child.\n");
			exit(1);
		}
		if(servreadstatus == 0){
			printf("Monitor exiting server read fail\n");
			exit(1);
		}
		if(servreadstatus > 0){
			userwritestatus = write(fd_write_to_user, servrbuf, strlen(servrbuf));

			memset(&servrbuf[0], 0, sizeof(servrbuf));

			if(userwritestatus == -1){
				perror("Child process failed to write to user. Terminating child.\n");
				exit(1);
			}
			if(userwritestatus == 0){
				printf("Monitor exiting user write fail\n");
				exit(1);
			}
		}

		// read from user, send to server
		int userreadstatus;
		int servwritestatus;
		char userrbuf[MAX_MSG];

		userreadstatus = read(fd_read_from_user, userrbuf, sizeof(userrbuf));

		if(userreadstatus == -1 && errno != EAGAIN){
			perror("Child process failed to read from user. Terminating child.\n");
			exit(1);
		}
		if(userreadstatus == 0){
			printf("Monitor exiting user read fail\n");
			exit(1);
		}
		if(userreadstatus > 0){
			servwritestatus = write(fd_write_to_server, userrbuf, strlen(userrbuf));

			memset(&userrbuf[0], 0, sizeof(userrbuf)); //Clears user buffer

			if(servwritestatus == -1){
				perror("Child process failed to write to server. Terminating child.\n");
				exit(1);
			}
			if(servwritestatus == 0){
				printf("Monitor exiting server write fail\n");
				exit(1);
			}
		}
		usleep(SLEEP_POLL_TIME);
	}
}
/*
 * Initialize a set of pipes for a new user connection
 */
void initialize_pipes(int p1[], int p2[]){
	if(-1 == pipe(p1)){
		perror("Failed to initialize pipe\n");
		exit(1);
	}
	if(-1 == pipe(p2)){
		perror("Failed to initialize pipe\n");
		exit(1);
	}
	if(-1 == fcntl(p1[0], F_SETFL, fcntl(p1[0], F_GETFL) | O_NONBLOCK)){
		perror("Failed to set pipe read end to nonblocking\n");
		exit(1);
	}
	if(-1 == fcntl(p2[0], F_SETFL, fcntl(p2[0], F_GETFL) | O_NONBLOCK)){
		perror("Failed to set pipe read end to nonblocking\n");
		exit(1);
	}
}

/*
 * Deal with a user attempting to connect to the server
 */
int process_connection(char* user_id,
		int pipe_MONITOR_reading_from_user[],
		int pipe_MONITOR_writing_to_user[],
		int pipe_SERVER_reading_from_monitor[],
		int pipe_SERVER_writing_to_monitor[],
	       	USER * user_list){
	int userwritestatus;
	if(-1 != find_user_index(user_list, user_id)){
		const char * dupusererr = "Username is in use. Connect with a different name.\n";
		userwritestatus = write(pipe_MONITOR_writing_to_user[1], dupusererr, strlen(dupusererr));
		if(userwritestatus <= 0){
			printf("Failed to inform user %s that they could not join.\n", user_id);
		}
		return -1;
	} else {
		int empty_slot = find_empty_slot(user_list);
		if(empty_slot == -1){
			const char * servfull = "Server is full.\n";
			userwritestatus = write(pipe_MONITOR_writing_to_user[1], servfull, strlen(servfull));
			if(-1 == userwritestatus){
				printf("Failed to inform user %s they could not join.\n", user_id);
			}
			return -1;
		}
		// create the user and add to userlist, start child monitor process
		pid_t pid = fork();
		if(pid == -1){
			fprintf(stderr,"failed to create a monitor process for %s\n", user_id);
			const char * forkfail = "Could not start monitor for your client.\n";
			userwritestatus = write(pipe_MONITOR_writing_to_user[1], forkfail, strlen(forkfail));
			exit(1);
		} else if (pid == 0){
			initialize_monitor(pipe_MONITOR_reading_from_user,
					pipe_MONITOR_writing_to_user,
					pipe_SERVER_reading_from_monitor,
					pipe_SERVER_writing_to_monitor);
		} else if (pid > 1){
			add_user(empty_slot, user_list, pid, user_id, pipe_SERVER_reading_from_monitor[0], pipe_SERVER_writing_to_monitor[1]);
			close_pipe_set(pipe_MONITOR_reading_from_user,
					pipe_MONITOR_writing_to_user);
			if((-1 == close(pipe_SERVER_reading_from_monitor[1])) ||
			    -1 == close(pipe_SERVER_writing_to_monitor[0])){
				printf("Failed to close pipes");
				exit(1);
			}
		}
	}
	return 0;
}

/*
 * Deal with a user command
 */
void process_user_command(char * monitorbuf, USER * user_list, int userIndex)
{
	int i;

	//Removes newline characters from the user command buffer
	for(i=0; i<strlen(monitorbuf); i++)
	{
		if(monitorbuf[i] == '\n')
		{
			monitorbuf[i] = '\0';
		}
	}

	char *tokens[MAX_TOKEN_COUNT]; char input[MAX_MSG];
	strcpy(input, monitorbuf);
	int token_count = parse_line(input, tokens, " ");

	ssize_t writeStat;

	if(strcmp(tokens[0], "\\p2p") == 0)
	{
		send_p2p_msg(userIndex, user_list, monitorbuf);
	}
	else if(strcmp(tokens[0], "\\list") == 0)
	{
		list_users(userIndex, user_list);
	}
	else if(strcmp(tokens[0], "\\exit") == 0)
	{
		printf("\nUser exiting server: %s\n", user_list[userIndex].m_user_id);
		kick_user(userIndex, user_list);
		print_prompt("admin");
	}
	else if(tokens[0][0] == '\\')
	{
		//Invalid command
		strcpy(input, "Invalid command");
		writeStat = write(user_list[userIndex].m_fd_to_user, input, sizeof(input));

		if(writeStat == -1){perror("Write failed");exit(1);}
	}
	else
	{
		//Broadcast to all users
		broadcast_msg(user_list, monitorbuf, user_list[userIndex].m_user_id);
	}
}
/*
 * Deal with a server command
 */
void process_server_command(char * cmd, int cmd_length, USER * user_list){
	char * tokens[MAX_TOKEN_COUNT];
	char cpycmd[MAX_MSG];
        strncpy(cpycmd, cmd, cmd_length);
	cpycmd[cmd_length] = '\0';
	int token_count  = parse_line(cpycmd, tokens, " ");
	//printf("token count: %d\ntokens[0]: %s\ntokens[1]: %s\n", token_count, tokens[0],tokens[1]);
	if(0 == strcmp(tokens[0], "\\list")){
		printf("\n");
		list_users(-1, user_list);
	}
	else if(0 == strcmp(tokens[0], "\\kick")){
		if(token_count < 2){
			printf("No user specified to kick command. Try \\kick <user>\n");
		} else {
			int user_idx = find_user_index(user_list, tokens[1]);
			if(-1 == user_idx){
				printf("Specified user not found.");
			} else {
				printf("Kicking user: %s", tokens[1]);
				kick_user(user_idx, user_list);
			}
		}

	}
	else if(0 == strcmp(tokens[0], "\\exit")){
		exit(0);
	}
	else if(tokens[0][0] == '\\'){
		printf("Invalid server command. Valid commands are <message>, \\list, \\kick <user>, and \\exit.");
	}
	else if(token_count > 0){
		//broadcast the message to all users
		for(int i = 0; i < MAX_USER; i++){
			if(user_list[i].m_status){
				continue;
			}
			const char notice_prefix[] = "Notice: ";
			char notice_buf[MAX_MSG];
			strncpy(notice_buf, notice_prefix, sizeof(notice_prefix) - 1);
			strncpy(notice_buf + sizeof(notice_prefix) - 1, cmd, cmd_length);
			int writestatus;
			writestatus = write(user_list[i].m_fd_to_user, notice_buf, sizeof(notice_prefix) - 1 + cmd_length);
			if(writestatus <= 0){
				printf("Server failed to broadcast to user: %s\n", user_list[i].m_user_id);
			}
			usleep(SLEEP_POLL_TIME);
		}
	}
}


/* ---------------------End of the functions that implementServer functionality -----------------*/


/* ---------------------Start of the Main function ----------------------------------------------*/
int main(int argc, char * argv[])
{
	int nbytes;
	setup_connection("4061_CHATROOM"); // Specifies the connection point as argument.

	USER user_list[MAX_USER];
	init_user_list(user_list);   // Initialize user list

	char stdin_buf[MAX_MSG];
	fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
	print_prompt("admin");

	int pipe_MONITOR_reading_from_user[2];
	int pipe_MONITOR_writing_to_user[2];
	int pipe_SERVER_reading_from_monitor[2];
	int pipe_SERVER_writing_to_monitor[2];
	char user_id[MAX_USER_ID];

	initialize_pipes(pipe_SERVER_reading_from_monitor,
			pipe_SERVER_writing_to_monitor);
	while(1) {
		/* ------------------------YOUR CODE FOR MAIN--------------------------------*/

		// PROCESSING NEW USER CONNECTIONS
		if(-1 != get_connection(user_id, pipe_MONITOR_writing_to_user, pipe_MONITOR_reading_from_user)){

			printf("processing new connection\n");
			fcntl(pipe_MONITOR_reading_from_user[0], F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
			fcntl(pipe_MONITOR_writing_to_user[0], F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
			initialize_pipes(pipe_SERVER_reading_from_monitor,
				pipe_SERVER_writing_to_monitor);

			if(-1 == process_connection(user_id,
				pipe_MONITOR_reading_from_user,
				pipe_MONITOR_writing_to_user,
				pipe_SERVER_reading_from_monitor,
				pipe_SERVER_writing_to_monitor,
				user_list)){

				//something went wrong, need to close pipes
				close_pipe_set(pipe_SERVER_reading_from_monitor,
						pipe_SERVER_writing_to_monitor);
			} else {
				printf("%s has successfully connected\n", user_id);
			}

			print_prompt("admin");
		}


		// PROCESSING USER COMMANDS
		// poll monitor processes, handle user commands if any
		// close user if input pipe write end is closed
		char monitorbuf [MAX_MSG];
		int readmonitorstatus;
		for(int i = 0; i < MAX_USER; i++){
			if(user_list[i].m_status){
				continue;
			}
			readmonitorstatus = read(user_list[i].m_fd_to_server, monitorbuf, sizeof(monitorbuf));
			if(readmonitorstatus == -1 && errno != EAGAIN){
				perror("");
				fprintf(stderr, "Monitor for user %s failed with error \n", user_list[i].m_user_id);
				kick_user(i, user_list);
				print_prompt("admin");
			}
			if(readmonitorstatus == 0){
				fprintf(stderr, "User %s has disconnected.\n", user_list[i].m_user_id);
				kick_user(i, user_list);
				print_prompt("admin");
			}
			if(readmonitorstatus > 0){
				process_user_command(monitorbuf, user_list, i);
			}
			memset(&monitorbuf[0], 0, sizeof(monitorbuf));
			usleep(SLEEP_POLL_TIME);
		}

		// PROCESSING SERVER COMMANDS
		// Poll stdin (input from the terminal) and handle admnistrative command
		int readstat = read(0, stdin_buf, MAX_MSG);
		if(readstat == 1){
			print_prompt("admin");
		}
		else if(-1 != readstat && readstat > 2){
			printf("\nServer command Received\n");
			stdin_buf[readstat - 1] = '\0';
			process_server_command(stdin_buf, readstat - 1, user_list);
			printf("\n");
			print_prompt("admin");
		}
		else if (-1 == readstat && errno != EAGAIN || 0 == readstat){
			perror("Server failed to read from stdin\n");
			exit(1);
		}
		usleep(SLEEP_POLL_TIME);
		/* ------------------------YOUR CODE FOR MAIN--------------------------------*/
	}
}

/* --------------------End of the main function ----------------------------------------*/
