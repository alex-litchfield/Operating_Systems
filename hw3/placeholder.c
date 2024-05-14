#include <stdio.h>
#include <pthread.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

//Required global variables by assignment
int*** dead_end_boards; 
pthread_t mainThread; 
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; //mutex
int max_squares; 
int x; 
int dead_end_boards_length; 
int dead_end_boards_count;


//Struct which holds as passing information into threads
struct processNextMoveArgs {
	int** board;
	int currMove;
	int currPos[2];
	int seen;
	int m;
	int n;
};

void freeBoardSpace(int** board, int m) {
	for (int i = 0; i < m; i++){
		free(board[i]);
	}
	free(board);
}

void freeNextMoves(int** nextMoves) {
	//Frees memory
	for (int i = 0; i < 8; i++){
		free(nextMoves[i]);
	}
	free(nextMoves);
}

void errorMessages() {
	fprintf(stderr, "ERROR: Invalid argument(s)\n");
	fprintf(stderr, "USAGE: a.out <m> <n> [<x>]\n");
}

//Locates potential moves and sets the coordinates for the next move to be that move
void setNextMove(int** nextMoves, int** board, int* currPos, int* numMoves, int* offsetArray, int n, int m, int rowCounter) {
	if ((currPos[0] + offsetArray[0]) >= 0 && (currPos[0]  + offsetArray[0]) < m  && (currPos[1] + offsetArray[1]) >= 0 && (currPos[1] + offsetArray[1]) < n && board[currPos[0] + offsetArray[0]][currPos[1] + offsetArray[1]] == 0){
        (*numMoves)++; 
        nextMoves[rowCounter][0] = currPos[0] + offsetArray[0]; 
        nextMoves[rowCounter][1] = currPos[1] + offsetArray[1];		
	}
    else {
        nextMoves[rowCounter][0] = 0;
        nextMoves[rowCounter][1] = 0;
    }
}

//Processes the next move provided by "SetNextMove()" - threading occurs here
void* processNextMove(void* args){
	//Struct initializing
	struct processNextMoveArgs* args1 = args;
	int** board = args1->board;
	int currMove = args1->currMove;
    int currPos[] = {args1->currPos[0], args1->currPos[1]};
	int seen = args1->seen;	
	int m = args1->m;
	int n = args1->n;
	int highest = currMove;
	int* returnSize = malloc(1*sizeof(int));
	*returnSize = 0;
	int numMoves = 0; //num moves left
	int** nextMoves = calloc(8, sizeof(int*));
	for (int i = 0; i < 8; i++){
		nextMoves[i] = calloc(2, sizeof(int));
	}

	//Prints solving message if we are on the first move
	if (currMove == 1){
		printf("THREAD %ld: Solving Sonny's knight's tour problem for a %dx%d board\n", pthread_self(), m, n);
	}
    
	//Searches potential moves via setNextMove()
	int offsetArray[8][2] ={ {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}, {1, 2}, {2, 1}, {2, -1}, {1, -2} };
	for (int i = 0; i < 8; i++) {
		setNextMove(nextMoves, board, currPos, &numMoves, offsetArray[i], n, m, i);
	}

	//This branch occurs when no there are no more possible moves
	if (numMoves == 0){
        //This branch occurs when every part of the board has been seen
		if (seen == m * n){
			printf("THREAD %ld: Sonny found a full knight's tour!\n", pthread_self());
			// Free the board 
			freeBoardSpace(board, m);
		}
		//This branch occurs when a dead end has been met (we have NOT seen every part of the board)
		else if (seen < m * n){
			printf("THREAD %ld: Dead end after move #%d\n", pthread_self(), currMove);
            if (seen < x){
				freeBoardSpace(board, m);
            }
            else {
                pthread_mutex_lock(&mutex);
                //Updating counters and adding to dead end board array
                if (dead_end_boards_count >= dead_end_boards_length){
                    dead_end_boards = realloc(dead_end_boards, (dead_end_boards_length + 10) * sizeof(int***));
                    dead_end_boards_length = dead_end_boards_length + 10;
                }
                dead_end_boards[dead_end_boards_count] = board; 
                dead_end_boards_count = dead_end_boards_count + 1;
                pthread_mutex_unlock(&mutex);
            }
		}
		
		//Updating global max squares value
		if (currMove > max_squares){
			max_squares = currMove;
		}

		//Freeing memory
		freeNextMoves(nextMoves);
		free(args1);

		*returnSize = currMove;
		pthread_exit(returnSize);
	}

	//This branch occurs if there is exactly only 1 possible move left
	else if (numMoves == 1){
		//Dedicating new memory to new instance of struct
		struct processNextMoveArgs* args = malloc(sizeof(struct processNextMoveArgs));
		args->currMove = currMove + 1;
		args->seen = seen + 1;
		args->m = m;
		args->n = n;

		//Loops through the board searching for the next move, and then actually places it into the board and struct
		int found_condition = 0;
		for (int i = 0; i < 8; i++){
			if (!(nextMoves[i][0] == 0 && nextMoves[i][1] == 0) && found_condition == 0){
				found_condition = 1;
				board[nextMoves[i][0]][nextMoves[i][1]] = currMove + 1;
				args->board = board;
				args->currPos[0] = nextMoves[i][0];
				args->currPos[1] = nextMoves[i][1];
			}
			//Frees some memory
			free(nextMoves[i]);
		}

		//Freeing memory
		free(nextMoves);
		free(returnSize);
		free(args1);

		//Calls the function again for the new args
		processNextMove(args);
	}

	//This branch occurs when there is more than 1 possible move left
	else if (numMoves > 1){
		printf("THREAD %ld: %d moves possible after move #%d; creating threads...\n", pthread_self(), numMoves, currMove);
		pthread_t tid[8];

		//Loops through the board searching for the next move, and then makes a new board for the move
		for (int i = 0; i < 8; i++){
			if (!(nextMoves[i][0] == 0 && nextMoves[i][1] == 0)){

				//Creates a new board 
				int** newBoard = calloc(m, sizeof(int*));
				for (int j = 0; j < m; j++){
					newBoard[j] = calloc(n, sizeof(int));
					for (int k = 0; k < n; k++){
						newBoard[j][k] = board[j][k];
					}
				}
				newBoard[nextMoves[i][0]][nextMoves[i][1]] = currMove + 1;

				//Dedicating new memory to new instance of struct
				struct processNextMoveArgs* args = malloc(sizeof(struct processNextMoveArgs));
				args->board = newBoard;
				args->currMove = currMove + 1;
				args->currPos[0] = nextMoves[i][0];
				args->currPos[1] = nextMoves[i][1];
				args->seen = seen + 1;
				args->m = m;
				args->n = n;

				//Creates new thread
				pthread_create(&tid[i], NULL, &processNextMove, args);

				#ifdef NO_PARALLEL 
				void* returnValue = malloc(0);
				free(returnValue);
				pthread_join(tid[i], (void**) &returnValue);
				if (*(int*) returnValue > highest){
					highest = *(int*) returnValue;
				}	
				printf("THREAD %ld: Thread [%ld] joined (returned %d)\n", pthread_self(), tid[i], *(int*) returnValue);
				free(returnValue);
				#endif

			}
		}

		#ifndef NO_PARALLEL
			//Children must be processed to execute
			for (int i = 0; i < 8; i++){
				if (nextMoves[i][0] != 0 && nextMoves[i][1] != 0){
					void* returnValue = malloc(0); 
					free(returnValue);
					pthread_join(tid[i], (void**) &returnValue);		
					if (*(int*) returnValue > highest){
						highest = *(int*) returnValue;
					}			
					printf("THREAD %ld: Thread [%ld] joined (returned %d)\n", pthread_self(), tid[i], highest);
					free(returnValue);
				}
			}
	
		#endif

		//Frees memory
		freeNextMoves(nextMoves);
		free(args1);
	}

	//If this is the main thread this branch is executed
	if (pthread_self() != mainThread){
		*returnSize = currMove;
		if (highest > *returnSize){
			*returnSize = highest;
		}
		//Freeing board
		freeBoardSpace(board, m);
		pthread_exit(returnSize);
	}
	free(returnSize);
	return returnSize;
}

int main(int argc, char** argv){
    //Requested for submitty: will uncomment if its needed
	setvbuf( stdout, NULL, _IONBF, 0 );

	//Error checking
	//Validate that there are at least 3 arguments, if not send an error
	if (argc < 3){
		errorMessages();
		return EXIT_FAILURE;	
	}

	int m = atoi(argv[1]);
	int n = atoi(argv[2]);
	//Validate that m and n are greater than 2, if not send an error
	if (!(m > 2 && n > 2)){
		errorMessages();
		return EXIT_FAILURE;	
	}
	//If given x we store it, otherwise the value is 0. We also validate that it is not grater than the size of the board
	if (argc == 4) {
        x = atoi(argv[3]);
		if (x > m * n){
			errorMessages();
			return EXIT_FAILURE;	
		}
	} 
    else {
        x = 0;
    }

	//Creating the board dynamically as required 
	int** board = calloc(m, sizeof(int*));
	for (int i = 0; i < m; i++){
		board[i] = calloc(n, sizeof(int));
		for (int j = 0; j < n; j++){
			board[i][j] = 0;
		}
	}
	board[0][0] = 1; 

	//Setting up the global variables and dedicating new memory to new instance of struct
	max_squares = 0;
	dead_end_boards = calloc(10, sizeof(int**));
	dead_end_boards_length = 10; 
	dead_end_boards_count = 0;
	struct processNextMoveArgs* args = malloc(sizeof(struct processNextMoveArgs));
	args->board = board;
	args->currMove = 1;
	args->currPos[0] = 0;
	args->currPos[1] = 0;
	args->seen = 1;
	args->m = m;
	args->n = n;

	//Creating the main thread, calling process args and printing results
	mainThread = pthread_self();
	processNextMove(args);
	printf("THREAD %ld: Best solution(s) found visit %d squares (out of %d)\n", pthread_self(), max_squares, n * m);

	//Print the dead end boards info
    printf("THREAD %ld: Dead end boards:\n", pthread_self());
	for (int i = 0; i < dead_end_boards_count; i++){
		printf("THREAD %ld: > ", pthread_self());
		for (int j = 0; j < m; j++){
			if (j != 0){
				printf("THREAD %ld:   ", pthread_self());
			}
			for (int k = 0; k < n; k++){
				if (dead_end_boards[i][j][k] == 0){
					printf(".");
				}
				else {
					printf("S");
				}
			}
		printf("\n");
		}
	}

	//Frees memory with dead end boards and the regular board
	for (int i = 0; i < dead_end_boards_count; i++){
		for (int j = 0; j < m; j++){
			free(dead_end_boards[i][j]);
		}
		free(dead_end_boards[i]);
	}
	free(dead_end_boards);
	pthread_mutex_destroy(&mutex);
	freeBoardSpace(board, m);

	return EXIT_SUCCESS;
}