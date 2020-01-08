/* CSci4061 F2018 Assignment 1
* login: cselabs login name (login used to submit)
* date: 10/05/18
* name: Lee Wintervold, Jack Bartels, Joe Keene
* id: Winte413, Barte285, keen0129
*/

// This is the main file for the code
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "util.h"

/*-------------------------------------------------------HELPER FUNCTIONS PROTOTYPES---------------------------------*/
void show_error_message(char * ExecName);
//Write your functions prototypes here
void show_targets(target_t targets[], int nTargetCount);
void build(target_t * basetarget, target_t * target, int nTargetCount);
int depend_are_newer(target_t *target);
/*-------------------------------------------------------END OF HELPER FUNCTIONS PROTOTYPES--------------------------*/


/*-------------------------------------------------------HELPER FUNCTIONS--------------------------------------------*/

//This is the function for writing an error to the stream
//It prints the same message in all the cases
void show_error_message(char * ExecName)
{
	fprintf(stderr, "Usage: %s [options] [target] : only single target is allowed.\n", ExecName);
	fprintf(stderr, "-f FILE\t\tRead FILE as a makefile.\n");
	fprintf(stderr, "-h\t\tPrint usage and exit.\n");
	exit(0);
}

//Write your functions here

int depend_are_newer(target_t *target){
	//returns 1 if dependencies are newer, 0 if otherwise 
	  int j;
	  //comparing target timestamp with its input files timestamp
	  for(j = 0; j < target->DependencyCount; j++){
		 char* tarname = target->TargetName;
		 char* inputfile = target->DependencyNames[j];
		 int mod_time = compare_modification_time(tarname, inputfile);
		 if(mod_time == -1){
			 fprintf(stderr, "A dependency file could not be found for target: %s\n",
					 target->TargetName);
			 exit(1);
		 } 
		 if(mod_time == 2){
			 //input more recent then target. needs update. return to build function to run target command
			return 1;
	  	}
	 }
	 //target is up to date
	 return 0;
}

void build(target_t* basetarget, target_t* target, int nTargetCount){
	//This function calls build on each dependency for the target passed in
	//which exists in the build DAG.
	//If the passed in target has already been built by this program,
	//or it exists on file and its dependencies are not newer than it,
	//we do not build it. 
	if (target->Status) {return;}

	int i;

	for(i = 0; i < target->DependencyCount; i++){
		int dependencyindex = find_target(target->DependencyNames[i], basetarget, nTargetCount);
		if (dependencyindex > 0){
			build(basetarget, basetarget + dependencyindex, nTargetCount);
		}
	}

	if(does_file_exist(target->TargetName) != -1 //if file doesn't exist, we must build
	   && !depend_are_newer(target) // if dependencies are newer, we must build
	   && target->DependencyCount > 0 //if there are no dependencies, we must build
	   ) {return;}

	pid_t childpid;
	childpid = fork();
	if(childpid==-1){
		fprintf(stderr, "Fork failure at %s\n", target->TargetName);
		exit(1);
	}
	if(childpid==0){
		//next line allocates the maximum possible number of tokens
		char* tokens[1024]; 
		char command[1024]; 
		strcpy(command, target->Command);
		int numtokens = parse_into_tokens(command, tokens, " ");
		printf("%s\n", target->Command);
		execvp(*tokens, tokens);
		perror((const char*) target->TargetName);
		exit(1);
	}
	//wait until child terminates
	int status;
	wait(&status);
	if(WEXITSTATUS(status)){
		fprintf(stderr, "child exited with error code=%d\n", WEXITSTATUS(status));
		exit(1);
	}
	target->Status = 1;
}

//Phase1: Warmup phase for parsing the structure here. Do it as per the PDF (Writeup)
void show_targets(target_t targets[], int nTargetCount)
{
	int i;
	for(i = 0; i < nTargetCount; i++){
		printf("TargetName: %s\n", targets[i].TargetName);
		printf("DependencyCount: %d\n", targets[i].DependencyCount);
		printf("DependencyNames: ");
		int j;
		for(j = 0; j < targets[i].DependencyCount; j++){
			printf("%s", targets[i].DependencyNames[j]);
			if(j < targets[i].DependencyCount - 1){
				printf(", ");
			}
		}
		printf("\n");
		printf("Command: %s\n", targets[i].Command);
	}
	//Write your warmup code here

}

/*-------------------------------------------------------END OF HELPER FUNCTIONS-------------------------------------*/


/*-------------------------------------------------------MAIN PROGRAM------------------------------------------------*/
//Main commencement
int main(int argc, char *argv[])
{
  target_t targets[MAX_NODES];
  int nTargetCount = 0;
  int targetStartIndex = 0; //Index of target for final build

  /* Variables you'll want to use */
  char Makefile[64] = "Makefile";
  char TargetName[64];

  /* Declarations for getopt. For better understanding of the function use the man command i.e. "man getopt" */
  extern int optind;   		// It is the index of the next element of the argv[] that is going to be processed
  extern char * optarg;		// It points to the option argument
  int ch;
  char *format = "f:h";
  char *temp;

  //Getopt function is used to access the command line arguments. However there can be arguments which may or may not need the parameters after the command
  //Example -f <filename> needs a filename, and therefore we need to give a colon after that sort of argument
  //Ex. f: for h there won't be any argument hence we are not going to do the same for h, hence "f:h"
  while((ch = getopt(argc, argv, format)) != -1)
  {
	  switch(ch)
	  {
	  	  case 'f':
	  		  temp = strdup(optarg);

			  if(strlen(temp) >= 63)
			  {
			  	printf("Input Makefile name exceeds length limit.");
				free(temp);
				exit(1);
			  }

	  		  strcpy(Makefile, temp);  // here the strdup returns a string and that is later copied using the strcpy
	  		  free(temp);	//need to manually free the pointer
	  		  break;

	  	  case 'h':
		  	  printf("Usage: ./make4061 [flags] [target]\n\n"
			  	 "Flags:\n"
				 "\t-f <makefile file name>   :   Specifies the makefile to be used for building\n"
				 "\t-h                        :   Prints usage statement and flags\n\n"
				 "Target: specifies which target in the makefile to build; defaults to first target\n");
			  exit(0);
	  	  default: //Usage statement upon improper usage
			  show_error_message(argv[0]);
	  		  exit(1);
	  }

  }

  argc -= optind;
  if(argc > 1)   //Means that we are giving more than 1 target which is not accepted
  {
	  show_error_message(argv[0]);
	  return -1;   //This line is not needed
  }

  /* Init Targets */
  memset(targets, 0, sizeof(targets));   //initialize all the nodes first, just to avoid the valgrind checks

  /* Parse graph file or die, This is the main function to perform the toplogical sort and hence populate the structure */
  if((nTargetCount = parse(Makefile, targets)) == -1)  //here the parser returns the starting address of the array of the structure. Here we gave the makefile and then it just does the parsing of the makefile and then it has created array of the nodes
	  return -1;


  //Phase1: Warmup-----------------------------------------------------------------------------------------------------
  //Parse the structure elements and print them as mentioned in the Project Writeup
  /* Comment out the following line before Phase2 */
  //show_targets(targets, nTargetCount);
  //End of Warmup------------------------------------------------------------------------------------------------------

  /*
   * Set Targetname
   * If target is not set, set it to default (first target from makefile)
   */
  if(argc == 1)
	strcpy(TargetName, argv[optind]);    // here we have the given target, this acts as a method to begin the building
  else
  	strcpy(TargetName, targets[0].TargetName);  // default part is the first target

  /*
   * Now, the file has been parsed and the targets have been named.
   * You'll now want to check all dependencies (whether they are
   * available targets or files) and then execute the target that
   * was specified on the command line, along with their dependencies,
   * etc. Else if no target is mentioned then build the first target
   * found in Makefile.
   */

  //Phase2: Begins ----------------------------------------------------------------------------------------------------
  /*Your code begins here*/

  targetStartIndex = find_target(TargetName, &targets[0], nTargetCount);

  if(targetStartIndex == -1)
  {
  	printf("%s is not a valid target.\n", TargetName);
	exit(1);
  }

  build(&targets[targetStartIndex], &targets[targetStartIndex], nTargetCount-targetStartIndex);

  /*End of your code*/
  //End of Phase2------------------------------------------------------------------------------------------------------

  return 0;
}
/*-------------------------------------------------------END OF MAIN PROGRAM------------------------------------------*/
