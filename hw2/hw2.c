#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>

//Checks the number of child processes and updates the counter if it is outdated
void checkChildProcesses (int* activeChildProcesses) {
	for (int i = 0; i < *activeChildProcesses; i++) {
		int status; 
		int activepid = waitpid(-1, &status, WNOHANG);
		if (waitpid(-1, &status, WNOHANG) != 0) { //Returns info on child status immediately
			if (WIFSIGNALED(status) == true) {
				printf("[process %d terminated abnormally]\n", activepid);
			}
			else {
				printf("[process %d terminated with exit status %d]\n", activepid, WEXITSTATUS(status));
			}
			*activeChildProcesses = *activeChildProcesses - 1;
		}
	}
}

//Deallocates the tokenArray and any user input
void deallocateTokenArray(int containerSize, char** tokenArray, char* input) {
	for (int i = 0; i < containerSize; i++) {
		if (tokenArray[i] != NULL) {
			free(tokenArray[i]);
		}
	}
	free(tokenArray);
	free(input);
}

//Parses the given command token and returns the number of tokens
int parseCommand(char** tokenArray, char* input) {
	int counter = 0;
	char* origToken = strtok(input, " ");
	while (origToken != NULL) {
		char* activeToken = calloc(65, sizeof(char)); //Assumes the max size is 64 chars + 1 end char
		strcpy(activeToken, origToken);
		tokenArray[counter] = activeToken;
		counter = counter + 1;
		origToken = strtok(NULL, " ");
	}
	return counter;
}

//Handles CD command (cd, cd /, cd (arg) )
void commandCD(int tokenCounter, char** currentWorkingPath, char** tokenArray) {
	//Situation where the command "cd"
	if (tokenCounter == 1) {
		if (chdir(getenv("HOME")) == 0) {
			*currentWorkingPath = (char*) realloc(*currentWorkingPath, strlen(getenv("HOME") + 1) * sizeof(char));
			strcpy(*currentWorkingPath, getenv("HOME"));
		}
	}
	//Situation where the command is "cd /"
	else if (strcmp(tokenArray[1], "/") == 0) {
		if (chdir("/") == 0) {
			*currentWorkingPath = (char*) realloc(*currentWorkingPath, (pathconf(".", _PC_PATH_MAX) + 1) * sizeof(char));
			getcwd(*currentWorkingPath, pathconf(".", _PC_PATH_MAX));
		}
	}
	//Situation where the command is "cd (other argument here)"
	else {
		*currentWorkingPath = (char*) realloc(*currentWorkingPath, (pathconf(".", _PC_PATH_MAX) + 1) * sizeof(char));
		getcwd(*currentWorkingPath, pathconf(".", _PC_PATH_MAX));
		if (chdir(tokenArray[1]) == 0) {
			*currentWorkingPath = (char*) realloc(*currentWorkingPath, (pathconf(".", _PC_PATH_MAX) + 1) * sizeof(char));
			getcwd(*currentWorkingPath, pathconf(".", _PC_PATH_MAX));
		}
		else{
			fprintf(stderr, "chdir() failed: Not a directory\n");
		}
	}
}

//Locating the command executable if a pipe is present
void locateUsingPipe(char** tokenArray, int pipeNumber, int pathCounter, int execPathIndex[], char** pathArray, bool execArray[]) {
	for (int i = 0; i < 2; i++) {
		int commandNum = 0;
		if (i == 1) {
			commandNum = pipeNumber + 1;
		}
		for (int j = 0; j < pathCounter; j++) {
			if (pathArray[j] == NULL) {
				break;
			}
			char* temp = calloc(200, sizeof(char));
			strcpy(temp, pathArray[j]);
			temp[strlen(temp)] = '/';
			execPathIndex[i] = j;
			for (int k = 0; k < strlen(tokenArray[commandNum]); k++) {
				temp[strlen(temp)] = tokenArray[commandNum][k];
			}
			struct stat buf;
			int stats = lstat(temp, &buf);
			free(temp);
			if (stats == 0) {
				if (buf.st_mode & S_IXUSR) {
					if (i == 0) {
						execArray[1] = 1;
					}
					if (i == 1) {
						execArray[2] = 2;
					}
				}
				break;
			}
		}
	}
}

//Executes command if command executable is found and pipe is present
int executeUsingPipe(int* tokenCounter, char** tokenArray, int pipeNumber, int* childProcesses, char** pathArray, int execPathIndex[]) {
	//Determine if any background processes are going
	bool backgroundProcessRunning = false; 
	if (strcmp(tokenArray[*tokenCounter-1], "&") == 0) {
		*tokenCounter = *tokenCounter - 1;
		free(tokenArray[*tokenCounter]);
		tokenArray[*tokenCounter] = '\0';
		backgroundProcessRunning = true;
	}		 
	//Create the executable paths
	char* firstExec = calloc(strlen(tokenArray[0]) + strlen(pathArray[execPathIndex[0]]) + 2, sizeof(char));
	char* secondExec = calloc(strlen(tokenArray[pipeNumber + 1]) + strlen(pathArray[execPathIndex[1]]) + 2, sizeof(char));
	strcpy(firstExec, pathArray[execPathIndex[0]]);
	strcpy(secondExec, pathArray[execPathIndex[1]]);
	firstExec[strlen(firstExec)] = '/';
	secondExec[strlen(secondExec)] = '/';
	for (int i = 0; i < strlen(tokenArray[0]); i++) {
		firstExec[strlen(firstExec)] = tokenArray[0][i];
	}
	for (int i = 0; i < strlen(tokenArray[pipeNumber + 1]); i++) {
		secondExec[strlen(secondExec)] = tokenArray[pipeNumber + 1][i];
	}
	firstExec[strlen(firstExec)] = '\0';
	secondExec[strlen(secondExec)] = '\0';
	//Creates arguments for commands
	int firstExecArgsCounter = 0;
	int secondExecArgsCounter = 0;
	char** firstExecArgsArray = calloc(pipeNumber + 1, sizeof(char*));
	char** secondExecArgsArray = calloc(*tokenCounter - pipeNumber, sizeof(char*));
	for (int i = 0; i < pipeNumber; i++) {
		firstExecArgsCounter = firstExecArgsCounter + 1;
		firstExecArgsArray[i] = calloc(strlen(tokenArray[i]) + 1, sizeof(char));
		strcpy(firstExecArgsArray[i], tokenArray[i]);
	}
	for (int i = pipeNumber + 1; i < *tokenCounter; i++) {
		secondExecArgsArray[secondExecArgsCounter] = calloc(strlen(tokenArray[i]) + 1, sizeof(char));
		strcpy(secondExecArgsArray[secondExecArgsCounter], tokenArray[i]);
		secondExecArgsCounter = secondExecArgsCounter + 1;
	}

	//Creating pipe and checking if creation fails
	int pipefd[2]; 
	int activePipe = pipe(pipefd);
	if (activePipe == -1) {
		perror("Pipe creation failed\n");
		return EXIT_FAILURE;
	}
	//Begins execution, using fork, and checks if fork fails
	pid_t processID1 = fork();
	if (processID1 == -1) {
		perror("Failed to fork...\n");
		return EXIT_FAILURE;
	}
	else if (processID1 == 0) { //If fork succeeds, close the read file descriptor
		close(pipefd[0]);
		close(1);
		dup2(pipefd[1], 1);
		close(pipefd[1]);
		execv(firstExec, firstExecArgsArray);
		perror("EXEC ONE FAILED\n");
		return EXIT_FAILURE;
	}
	//Begins execution of 2nd fork and checks for if it fails
	pid_t processID2 = fork();
	if (processID2 == -1) {
		perror("Failed to fork...\n");
		return EXIT_FAILURE;
	}
	if (processID1 > 0 && processID2 == 0) { //If fork succeeds, close the write file descriptor
		close(pipefd[1]);
		close(0);
		dup2(pipefd[0], 0);	
		close(pipefd[0]);
		execv(secondExec, secondExecArgsArray);
		perror("EXEC TWO FAILED\n");
		return EXIT_FAILURE;
	}
	if (processID1 > 0 && processID2 > 0) { //Parent process operations
		close(pipefd[0]);
		close(pipefd[1]);
		if (backgroundProcessRunning == true) {
			printf("[running background process \"%s\"]\n", tokenArray[0]);
			printf("[running background process \"%s\"]\n", tokenArray[*tokenCounter - pipeNumber]);
			*childProcesses = *childProcesses + 2;
			if (waitpid(-1, NULL, WNOHANG) == -1 || waitpid(-1, NULL, WNOHANG) == -1) {
				perror("waitpid() error\n");
				return EXIT_FAILURE;
			}
		}
		else {
			waitpid(processID2, NULL, WUNTRACED | WCONTINUED);
			waitpid(processID1, NULL, WUNTRACED | WCONTINUED);
		}
		//Freeing any still allocated memory
		free(firstExec);
		free(secondExec);
		for (int i = 0; i < firstExecArgsCounter + 1; i++) {
			free(firstExecArgsArray[i]);
		}
		for (int i = 0; i < secondExecArgsCounter  + 1; i++) {
			free(secondExecArgsArray[i]);
		}
		free(firstExecArgsArray);
		free(secondExecArgsArray);

	}
	else { //Not a parent process
		exit(1);
	}
	return 0;
}

//Locating the command executable if a pipe is NOT present 
void locateWithoutPipe(char** tokenArray, char** pathArray, int pathCounter, int execPathIndex[], bool execArray[]) {
	for (int i = 0; i < pathCounter; i++) {	
		if (pathArray[i] == NULL) {
			break;
		}
		char* tempPath = calloc(200, sizeof(char));
		strcpy(tempPath, pathArray[i]);
		tempPath[strlen(tempPath)] =  '/';
		execPathIndex[0] = i;
		for (int j = 0; j < strlen(tokenArray[0]); j++) {
			tempPath[strlen(tempPath)] = tokenArray[0][j];
		}
		struct stat buf;
		int stats = lstat(tempPath, &buf);
		free(tempPath);
		if (stats== 0) {
			if (buf.st_mode & S_IXUSR) {
				execArray[0] = 1;
			} 
			break;
		}
	}
}

//Executes command if command executable is found and pipe is NOT present
int executeWithoutPipe(char** tokenArray, int execPathIndex[], char** pathArray, int tokenCounter, int* childProcesses) {
	//Determine if any background processes are going
	bool backgroundProcessRunning = false; 
	if (strcmp(tokenArray[tokenCounter-1], "&") == 0) {
		free(tokenArray[tokenCounter - 1]);
		tokenArray[tokenCounter - 1] = '\0';
		backgroundProcessRunning = true;
	}		 
	//Create the executable path
	char* exec = calloc(strlen(tokenArray[0]) + strlen(pathArray[execPathIndex[0]]) + 2, sizeof(char));
	strcpy(exec, pathArray[execPathIndex[0]]);
	exec[strlen(exec)] = '/';
	for (int i = 0; i < strlen(tokenArray[0]); i++) {
		exec[strlen(exec)] = tokenArray[0][i];
	}
	exec[strlen(exec)] = '\0';
	//Begins execution, using fork, and checks if fork fails
	pid_t pid = fork();
	if (pid == -1) {
		perror("Failed to fork...");
		return EXIT_FAILURE;
	}
	if (pid == 0) { //Child process Operations
		execv(exec, tokenArray);
		perror("EXEC FAILED\n");
		return EXIT_FAILURE;
	}
	else if (pid > 0) { //Parent process operations
		if (backgroundProcessRunning == true) {
			printf("[running background process \"%s\"]\n", tokenArray[0]);
			*childProcesses = *childProcesses + 1;
			if (waitpid(-1, NULL, WNOHANG) == -1) {
				perror("waitpid() error\n" );
				return EXIT_FAILURE;
			}
		}
		else {
			waitpid(pid, NULL, 0);
		}
	}
	free(exec);
	return 0;
}

int main() {
	setvbuf(stdout, NULL, _IONBF, 0); //Disables buffered output for grading on Submitty

	//Finds the current working directory and sets it to the current path variable
	char* currentWorkingPath = calloc(pathconf(".", _PC_PATH_MAX), sizeof(char));	
	currentWorkingPath = getcwd(currentWorkingPath, (size_t) pathconf(".", _PC_PATH_MAX));

	//Populates myPath with $MYPATH environment variable
	char* myPath = calloc(999, sizeof(char)); 
	if (getenv("MYPATH") != NULL) {
		strcpy(myPath, getenv("MYPATH")); 
	}
	else {
		strcpy(myPath, "/bin:.");
	}

	//Populates the path container array with character arrays which are paths seperated by ":"
	char** pathArray = calloc(100, sizeof(char*));
	char* currPath = strtok(myPath, ":"); 
	int pathCounter = 0;
	while (currPath != NULL) {
		pathArray[pathCounter] = calloc(100, sizeof(char)); 
		currPath[strlen(currPath)] = '\0';
		strcpy(pathArray[pathCounter], currPath);
		currPath = strtok(NULL, ":");
		pathCounter = pathCounter + 1;
	}

	//Initialize the number of child processes as 0 prior to execution of continuous while loop
	int childProcesses = 0; 
	while (1) {	
		//Checks the number of child processes and updates the counter if it is outdated
		checkChildProcesses(&childProcesses);
		
		char** tokenArray = calloc(20, sizeof(char*)); //Assumes the token container will not exceed 20 tokens
		char* input = calloc(1024, sizeof(char)); //Assumes that each command read in will not exceed 1024 characters 
		bool execArray[] = {false, false, false};
		int execPathIndex[2];
		int pipeNumber = 0;
		bool pipePresent = false;	
		
		//Prints current working directory and takes in user input
		printf("%s$ ", currentWorkingPath); //Prints the current working directory
		fgets(input, 1024, stdin); 
		input[strlen(input) - 1] = '\0'; //End character needed to prevent seg faults

		//If the input is "exit", we deallocate memory and exit the loop
		if (strcmp(input, "exit") == 0) {
			//Deallocates the tokenArray and any user input
			deallocateTokenArray(sizeof(tokenArray), tokenArray, input);
			break;
		}

		//Parses the given command token and returns the number of tokens
		int tokenCounter = parseCommand(tokenArray, input);

		//Checks if one of the tokens is a pipe character
		for (int i = 0; i < tokenCounter; i++) {
			if (strcmp(tokenArray[i], "|") == 0) {
				pipeNumber = i;
				pipePresent = true;
			}
		}

		//Checks if the command is a "cd" command
		if (strcmp(tokenArray[0], "cd") == 0) {
			//Handles CD command (cd, cd /, cd (arg) )
			commandCD(tokenCounter, &currentWorkingPath, tokenArray);
			//Deallocates the tokenArray and any user input
			deallocateTokenArray(tokenCounter, tokenArray, input);
			continue; 
		}

		//If a pipe is within the current command
		if (pipePresent == true) {
			//Locating the command executable if a pipe is present 
			locateUsingPipe(tokenArray, pipeNumber, pathCounter, execPathIndex, pathArray, execArray);
			//Prints an error if the command executable was not found
			if (execArray[1] == false || execArray[2] == false) {
				fprintf(stderr, "ERROR: one or both of the commands not found");
			}
			//If both command executables are found
			else if (execArray[1] == true && execArray[2] == true) {
				//Executes command if command executable is found and pipe is present
				executeUsingPipe(&tokenCounter, tokenArray, pipeNumber, &childProcesses, pathArray, execPathIndex);
			}
		}
		//If no pipe is found within the current command
		else {
			//Locating the command executable if a pipe is NOT present 
			locateWithoutPipe(tokenArray, pathArray, pathCounter, execPathIndex, execArray);
			//Prints an error if the command executable was not found
			if (execArray[0] == false) {
				fprintf(stderr, "ERROR: command \"%s\" not found\n", tokenArray[0]);
			}
			//If both command executables are found
			else if (execArray[0] == true) {
				//Executes command if command executable is found and pipe is NOT present
				executeWithoutPipe(tokenArray, execPathIndex, pathArray, tokenCounter, &childProcesses);
			} 
		}

		//Deallocates the tokenArray and any user input
		deallocateTokenArray(sizeof(tokenArray), tokenArray, input);
	}

	//Prints "bye" and deallocates the memory in myPath, currentWorkingPath, and pathArray
	printf("bye\n");
	free(myPath);
	free(currentWorkingPath);
	for (int i = 0; i < 50; i++) {
		if (pathArray[i] != NULL) {
			free(pathArray[i]);
		}
	}
	free(pathArray);

	return EXIT_SUCCESS;
}