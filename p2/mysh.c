#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#include <fcntl.h>

void flush(void){
	char c;
	while((c = getchar())!= '\n' && c != EOF)
		/* discard */ ;
}
// END OF FLUSH


void stripEndNewLine(char *usrInput) {
	int i = 0;
	
	// this loop helps to get last valid char entered
	while(usrInput[i] != '\0') {
		i++; 
	}
	
	if(usrInput[i-1] == '\r' || usrInput[i-1] == '\n') {
		usrInput[i-1] = '\0'; // replace carriage ret and newline with null
	}
}
// END OF STRIPENDNEWLINE

char *trimwhitespace(char *str)
{
  char *end;

  // Trim leading space
  while(isspace(*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace(*end)) end--;

  // Write new null terminator
  *(end+1) = 0;

  return str;
}
// END OF TRIMWHITESPACE

void printError() {
	char error_message[30] = "An error has occurred\n";
	write(STDERR_FILENO, error_message, strlen(error_message));
}
// END OF PRINTERROR

/* my own exit function */
void myExit(int status) {
	exit(status);
}
// END OF MYEXIT

/* my own cd function
 0 successful, -1 error */
int cd(char *args) {
	int retStatus;
	if(args != NULL) {
		retStatus = chdir(args);
	} else {
		// cd to home dir
		args = getenv("HOME"); 
		retStatus = chdir(args);
	}
	return retStatus;
} 
// END OF CD

/* my own pwd function
 0 on success, -1 if fails */
int pwd(void) {
	char cwdBuf[1024];
	char *pwdOut;
	
	pwdOut = getcwd(cwdBuf, sizeof(cwdBuf));
	
	if(pwdOut == NULL) { // if getcwd fail
		return -1;
	} else {
		write(STDOUT_FILENO, pwdOut, strlen(pwdOut));
		write(STDOUT_FILENO, "\n", 1);
		return 0;
	}
	return 0;
}
// END OF PWD

/* 0 if valid c file, -1 if invalid
 .c once for valid file */
int validC(char *inC) {

		char *dot = ".";
		if(strcmp(&(inC[0]),dot) == 0) { // filename start with .
			return -1;
		}
		
        char *dotC = ".c";
        int isValidC = -1;
        char *res = strstr(inC, dotC);
        
        if(res != NULL) {
            isValidC = 0; // still need to check if there is another .c substring
        } else {
        	return -1;
        }

        char *newS = strstr(res+2, dotC);
        
		// if there is atleast .c twice, invalid
        if(newS != NULL) {
                 return -1;
        } else {
                isValidC = 0;
        }
        
        return isValidC;
}
// END OF VALIDC

void redirExtract(char *token[512]) {
	char *redir = ">";
	char *firstRedir = strstr(token[0], redir);
	
	// if there is no '>', don't bother to proceed
	if(firstRedir == NULL) { 
		return;
	}
	
	int firstRedirIndex = firstRedir - token[0]; // pointer substraction to get index!
	
	char cmdToken[firstRedirIndex+1]; // extra one for null 
	strncpy(cmdToken, token[0], firstRedirIndex);
	cmdToken[firstRedirIndex] = '\0';
	
	// size of the substr after >
	int outFileLen = strlen(token[0]) - firstRedirIndex; // extra one for null
	char outputCmd[outFileLen];
	strcpy(outputCmd, firstRedir+1); // firstRedir + 1 is the substring after >
	
	token[0] = strdup(cmdToken);
	token[1] = strdup(redir);
	token[2] = strdup(outputCmd);
	
} // END OF REDIR EXTRACT

// helper method to redirect output 
// for case: cmd>out
void redirStdOut(int fd) {
        if(dup2(fd, fileno(stdout)) == -1) {
                printError();
        }
} // END OF REDIRSTDOUT

void freeStrDuped(char *token[512], int usedTokenLen) {
	int j;
	for(j = 0; j < usedTokenLen; j++) {
		free(token[j]);
	}
}

int main(int argc, char **argv) {

	char *batchFile = "no/such/file";
	int processMode = 0; // 0 for batch mode, 1 for interactive mode
	FILE * sourceStream = stdin;
	
	if(argc == 2) { // batch mode

		processMode = 0; // set to batch mode
		batchFile = strdup(argv[1]);
		sourceStream = fopen(batchFile, "r");
		free(batchFile);
		
		if(sourceStream == NULL) {
			printError();
			myExit(1);
		}

	} else if(argc == 1) {
		processMode = 1; // set to interactive mode
	} else { // error
		printError();
		myExit(1);
	}
	
	char usrInput[514];  
	char *myshStr = "mysh> ";
	//char *exitStr = "exit\n";
	char *newLine = "\n";
	char *redirStr = ">";
	char *cmdCd = "cd";
	char *cmdPwd = "pwd";
	char *cmdExit = "exit";
	char *token[512]; // very bad i know!
	int save_out = dup(fileno(stdout));
	int errorflag = -1;

	// interactive mode
	if(processMode == 1) {
		write(STDOUT_FILENO, myshStr, strlen(myshStr));
	}
	
	while(fgets(usrInput, sizeof(usrInput), sourceStream) != NULL) {
		
		// if the 512th char is not newline or null, user must have entered more!
		/*if(usrInput[513] != '\0' && usrInput[513] != '\n') {
			usrInput[513] = '\n';
			printError();
			flush();
		}*/
	/*	if(usrInput[512] == '\0'){
			if(usrInput[513]!='\n'){
				printError();
			}
		}*/
		if(strlen(usrInput) >= 512){
			if(usrInput[512]=='\0'){
				if(usrInput[513]!='\n'){
					printError();
					errorflag = 1;
				}
			}
			else if(usrInput[512]!='\0' && usrInput[512]!='\n'){
				if(usrInput[513]!='\n'){
					printError();
					errorflag = 1;
				}
			}
		}
		if(strlen(usrInput) == 1) { // empty command (carriage ret.)? continue!
			if(processMode == 1) {
				write(STDOUT_FILENO, myshStr, strlen(myshStr));
			} else {
				write(STDOUT_FILENO, newLine, strlen(newLine));
			}
			continue;
		}
		
		// print the cmd for batch mode
		if(processMode == 0) {
			write(STDOUT_FILENO, usrInput, strlen(usrInput));
		}
		
		stripEndNewLine(usrInput);
		//char *cleanInput = trimwhitespace(usrInput);
		
		char *cmdArg = strtok(usrInput, " ");
		
		if(cmdArg == NULL) { // user enter space only?
			if(processMode == 1) {
				write(STDOUT_FILENO, myshStr, strlen(myshStr));
			}
			continue;
		}
		
		int c = 0;
		
		while(cmdArg != NULL) {
			
			if(strcmp(cmdArg, newLine) != 0) {
				
				token[c] = strdup(cmdArg);
			} else { // if only \n, proceed to next input
				cmdArg = strtok(NULL, " "); // get next token
				continue; 
			}

			cmdArg = strtok(NULL, " ");
			c++; // increase counter
		}
		
		token[c] = NULL; // terminate with NULL!
					
		if(c == 1) { // if there is just one token, check for redir case without space
			redirExtract(token);
		}
		
		int i = 0;
		int redirCount = 0; // help to identify multiple '>'
		
		while(token[i] != NULL) {
			// check for '>'
			if(strcmp(token[i], redirStr) == 0) { 
				redirCount++;			
			}
			
			//printf("token[%d] : %s\n", i, token[i]);
			i++;
		}
					
		// if there are multiple '>' , print error 
		// and save time from trying to parse
		if(redirCount > 1) { 
			printError();
			if(processMode == 1) {
				write(STDOUT_FILENO, myshStr, strlen(myshStr));
			}
			continue;
		} else if(redirCount == 1) { // user try to redirect output
						
			// without redir source? error!
			if(strcmp(token[0], redirStr) == 0) {
				printError();
				if(processMode == 1) {
				write(STDOUT_FILENO, myshStr, strlen(myshStr));
				}
				continue;
			}
			
			// check whether out file arg exist
			if(strcmp(token[i-2], redirStr) == 0 &&
			token[i-1] != NULL && token[0] != NULL) {
				
				// redirect output here
				int fd = open(token[i-1], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
				
				if (fd == -1) {
					printError();
					if(processMode == 1) {
						write(STDOUT_FILENO, myshStr, strlen(myshStr));
					}
					continue;
				}
				
				if(dup2(fd, 1) == -1) { // output go to file
					printError();
					close(fd);
					if(processMode == 1) {
						write(STDOUT_FILENO, myshStr, strlen(myshStr));
					}
					continue;
				}
				
				token[i-2] = NULL;
				token[i-1] = NULL;
				close(fd);
	
			} else {
				printError();
				
				if(processMode == 1) {
					write(STDOUT_FILENO, myshStr, strlen(myshStr));
				}
				continue;
			}
		}
		
		//if print error is called, skip current process
		if(errorflag == 1){
			errorflag = -1;
			continue;
		}

		// check whether the cmd is built in or not
		if(strcmp(token[0], cmdCd) == 0) { // cd?
			int st = cd(token[1]);
			
			if(st == -1) {
				printError();
			}
		} else if (strcmp(token[0], cmdPwd) == 0) { // pwd?
			if(token[1] != NULL) {
				printError();
				myExit(0);
			}
			
			int st = pwd();
			
			if(st == -1) {
				printError();
			}
		} else if (strcmp(token[0], cmdExit) == 0) { // exit?
			if(token[1] != NULL) {
				printError();
				myExit(0);
			}
			
			myExit(0);
		} else if (validC(token[0]) == 0) { // fun features!
			
			int k;
			char *argRun[512]; // arguments for running compiled program!
			argRun[0] = "./a.out"; // generic output for gcc

			// <file.c> arg1
			// will cause argRun[1] = arg1 / argRun[1] = token[1]
			for(k = 1; k < i; k++) { 
				argRun[k] = token[k];
			}
			argRun[i] = '\0'; // null terminate the args
			
			// CHILD 1
			pid_t childpid = fork();
			
			if(childpid >= 0) { // succeed
		
				if(childpid == 0) { // child 2
				
					char *argCompile[512];
					argCompile[0] = "gcc";
					argCompile[1] = token[0];
					argCompile[2] = '\0';
					
					execvp(argCompile[0], argCompile);
					printError();
					myExit(1);
					
				} else { // parent
				
					// wait for child 1
					if(waitpid(childpid, NULL, 0) != childpid) {
					
						printError();
						
					} else { // need to run the compiled program
						
						// CHILD 2
						pid_t childpid2 = fork();
						
						if(childpid2 >= 0) { // succeed
							
							if(childpid2 == 0) { // child 2
								execvp(argRun[0], argRun);
								printError(); // error if return
								myExit(1);
							} else {
							
								// wait for child 2
								if(waitpid(childpid2, NULL, 0) != childpid2) {
									printError();
								}
							}
							
						} else {
						
							printError();
						}
						// END OF CHILD 2
					}
				}
			} else {
				printError();
			} 
			// END OF CHILD 1
			
		} else { // not built in, use child
			
			pid_t childpid = fork();
						
			if(childpid >= 0) { // succeed
				if(childpid == 0) { // child
					execvp(token[0], token);
					printError();
					myExit(1);
				} else { // parent
					// wait for child
					if(waitpid(childpid, NULL, 0) != childpid) {
						printError();
					}
				}
			} else {
				printError();
			}
			
		}
		
		redirStdOut(save_out);
		//flush(); // use my own flush!	
		if(processMode == 1) {
			write(STDOUT_FILENO, myshStr, strlen(myshStr));
		}		

	}
	
	myExit(0);
	
	return 0;
}
// END OF MAIN
