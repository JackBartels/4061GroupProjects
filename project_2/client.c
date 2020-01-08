#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include "comm.h"
#include "util.h"

/* -------------------------Main function for the client ----------------------*/
void main(int argc, char * argv[]) {		
	if(argc > 2){
		printf("Too many arguments\n");
		exit(0);
	}
	if(argc < 2){
		printf("No user name found\n");
		exit(0);
	}
	int pipe_user_reading_from_server[2], pipe_user_writing_to_server[2];
	char buf[MAX_MSG];                                                   //buffer for message
	char user_name[MAX_USER_ID];                                         
	strcpy(user_name, argv[1]);                                          //copy user name from command line to string
	fcntl(0, F_SETFL, fcntl(0, F_GETFL)| O_NONBLOCK);                    //make stdin nonblocking
	print_prompt(user_name);
	int segm;                                                            //return variable for checking "\seg" cammand
	char segcmd[5] = "\\seg";                                            //static char array for seg command
	ssize_t writeStat;

	// You will need to get user name as a parameter, argv[1].
	
	/* -------------- YOUR CODE STARTS HERE -----------------------------------*/
	if(connect_to_server("4061_CHATROOM", user_name, pipe_user_reading_from_server, pipe_user_writing_to_server) == 0) {
		if(-1 == close(pipe_user_reading_from_server[1]) || //close pipes not needed
		   -1 == close(pipe_user_writing_to_server[0])){
			printf("Failed to close a pipe.");
			exit(1);
		}
		//make pipe from server non-blocking
		fcntl(pipe_user_reading_from_server[0], F_SETFL, fcntl(pipe_user_reading_from_server[0], F_GETFL)| O_NONBLOCK); 
		while(1){
			
			int readstdin = read(0, &buf, MAX_MSG -1);           //read from stdin                
			if (readstdin == -1 && errno != EAGAIN){
				perror("Client failed to read from stdin");
				exit(1);
			}
			int segm = start_with(segcmd, buf);                  //check for "\seg" command
			if(segm == 0){
				int i = *(int*)0;
			}
			if(buf[0] == '\n'){
				print_prompt(user_name);                    //if user just hits enter do nothing
			}
			if (readstdin != -1 && buf[0] !='\n'){
				writeStat = write(pipe_user_writing_to_server[1], &buf, readstdin);
				if(-1 == writeStat){
					printf("User write to server failed");
					exit(1);
				}
				print_prompt(user_name);
			}
			memset(buf,0, MAX_MSG);                             //reset buffer
			usleep(SLEEP_POLL_TIME);
			int readserv = read(pipe_user_reading_from_server[0], &buf, MAX_MSG);  //check server input for message 
			if (readserv == -1 && errno != EAGAIN){
				perror("Client failed to read from server");
				exit(1);
			}
			if (readserv == 0){
				perror("Server connection closed. Exiting.");                      //server crashed, exit
				exit(1);
			}
			if (readserv != -1){
				printf("\n");
				writeStat = write(1, &buf, readserv);
				if(-1 == writeStat){
					printf("User failed to write to stdout");
					exit(1);
				}
				printf("\n");
				print_prompt(user_name);
			}
			memset(buf,0, MAX_MSG);                          //reset buffer
			usleep(SLEEP_POLL_TIME);
		}
	}
	/* -------------- YOUR CODE ENDS HERE -----------------------------------*/
}

/*--------------------------End of main for the client --------------------------*/


