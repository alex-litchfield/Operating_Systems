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

//Extra global variables being used
int x; //x argument
int dead_end_board_array_length; 
int num_dead_end_boards;


//Struct which holds as passing information into threads
struct processNextMoveArgs {
	int** board;
	int currentMoveNum;
	int currentXY[2];
	int squaresLocated; 
	int boardWidth;
	int boardHeight;
};

//Frees memory in Board container
void freeBoardSpace(int** board, int boardWidth) {
	for (int i = 0; i < boardWidth; i++){
		free(board[i]);
	}
	free(board);
}

//Frees memory in futureMoves container
void freeFutureMoves(int** futureMoves) {
	for (int i = 0; i < 8; i++){
		free(futureMoves[i]);
	}
	free(futureMoves);
}

void errorMessages() {
	fprintf(stderr, "ERROR: Invalid argument(s)\n");
	fprintf(stderr, "USAGE: a.out <m> <n> [<x>]\n");
}

//Locates potential moves and sets the coordinates for the next move to be that move
void setNextMove(int** futureMoves, int** board, int* currentXY, int* moves, int* offsetArray, int boardHeight, int boardWidth, int rowCounter) {
	if ((currentXY[0] + offsetArray[0]) >= 0 && (currentXY[0]  + offsetArray[0]) < boardWidth  && (currentXY[1] + offsetArray[1]) >= 0 && (currentXY[1] + offsetArray[1]) < boardHeight && board[currentXY[0] + offsetArray[0]][currentXY[1] + offsetArray[1]] == 0){
        (*moves)++; 
        futureMoves[rowCounter][0] = currentXY[0] + offsetArray[0]; 
        futureMoves[rowCounter][1] = currentXY[1] + offsetArray[1];		
	}
    else {
        futureMoves[rowCounter][0] = 0;
        futureMoves[rowCounter][1] = 0;
    }
}

//Processes the next move provided by "SetNextMove()" - threading occurs here
void* processNextMove(void* args){
	//Struct initializing
	struct processNextMoveArgs* args1 = args;
	int** board = args1->board;
	int currentMoveNum = args1->currentMoveNum;
    int currentXY[] = {args1->currentXY[0], args1->currentXY[1]};
	int squaresLocated = args1->squaresLocated;	
	int boardWidth = args1->boardWidth;
	int boardHeight = args1->boardHeight;
	int largest = currentMoveNum;
	int* returnSize = malloc(1*sizeof(int));
	*returnSize = 0;
	int moves = 0; //num moves left
	int** futureMoves = calloc(8, sizeof(int*));
	for (int i = 0; i < 8; i++){
		futureMoves[i] = calloc(2, sizeof(int));
	}

	//Prints solving message if we are on the first move
	if (currentMoveNum == 1){
		printf("THREAD %ld: Solving Sonny's knight's tour problem for a %dx%d board\n", pthread_self(), boardWidth, boardHeight);
	}
    
	//Searches potential moves via setNextMove()
	int offsetArray[8][2] ={ {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}, {1, 2}, {2, 1}, {2, -1}, {1, -2} };
	for (int i = 0; i < 8; i++) {
		setNextMove(futureMoves, board, currentXY, &moves, offsetArray[i], boardHeight, boardWidth, i);
	}

	//This branch occurs when no there are no more possible moves
	if (moves == 0){
        //This branch occurs when every part of the board has been seen
		if (squaresLocated == boardWidth * boardHeight){
			printf("THREAD %ld: Sonny found a full knight's tour!\n", pthread_self());
			// Free the board 
			freeBoardSpace(board, boardWidth);
		}
		//This branch occurs when a dead end has been met (we have NOT seen every part of the board)
		else if (squaresLocated < boardWidth * boardHeight){
			printf("THREAD %ld: Dead end after move #%d\n", pthread_self(), currentMoveNum);
            if (squaresLocated < x){
				freeBoardSpace(board, boardWidth);
            }
            else {
                pthread_mutex_lock(&mutex);
                //Updating counters and adding to dead end board array
                if (num_dead_end_boards >= dead_end_board_array_length){
                    dead_end_board_array_length = dead_end_board_array_length + 10;
                    dead_end_boards = realloc(dead_end_boards, (dead_end_board_array_length) * sizeof(int***));
                }
                dead_end_boards[num_dead_end_boards] = board; 
                num_dead_end_boards = num_dead_end_boards + 1;
                pthread_mutex_unlock(&mutex);
            }
		}
		
		//Updating global max squares value
		if (currentMoveNum > max_squares){
			max_squares = currentMoveNum;
		}

		//Freeing memory
		freeFutureMoves(futureMoves);
		free(args1);

		*returnSize = currentMoveNum;
		pthread_exit(returnSize);
	}

	//This branch occurs if there is exactly only 1 possible move left
	else if (moves == 1){
		//Dedicating new memory to new instance of struct
		struct processNextMoveArgs* args = malloc(sizeof(struct processNextMoveArgs));
		args->currentMoveNum = currentMoveNum + 1;
		args->squaresLocated = squaresLocated + 1;
		args->boardWidth = boardWidth;
		args->boardHeight = boardHeight;

		//Loops through the board searching for the next move, and then actually places it into the board and struct
		int found_condition = 0;
		for (int i = 0; i < 8; i++){
			if (!(futureMoves[i][0] == 0 && futureMoves[i][1] == 0) && found_condition == 0){
				found_condition = 1;
				board[futureMoves[i][0]][futureMoves[i][1]] = currentMoveNum + 1;
				args->board = board;
				args->currentXY[0] = futureMoves[i][0];
				args->currentXY[1] = futureMoves[i][1];
			}
			//Frees some memory
			free(futureMoves[i]);
		}

		//Freeing memory
		free(futureMoves);
		free(returnSize);
		free(args1);

		//Calls the function again for the new args
		processNextMove(args);
	}

	//This branch occurs when there is more than 1 possible move left
	else if (moves > 1){
		printf("THREAD %ld: %d moves possible after move #%d; creating threads...\n", pthread_self(), moves, currentMoveNum);
		pthread_t tid[8];

		//Loops through the board searching for the next move, and then makes a new board for the move
		for (int i = 0; i < 8; i++){
			if (!(futureMoves[i][0] == 0 && futureMoves[i][1] == 0)){

				//Creates a new board 
				int** newBoard = calloc(boardWidth, sizeof(int*));
				for (int j = 0; j < boardWidth; j++){
					newBoard[j] = calloc(boardHeight, sizeof(int));
					for (int k = 0; k < boardHeight; k++){
						newBoard[j][k] = board[j][k];
					}
				}
				newBoard[futureMoves[i][0]][futureMoves[i][1]] = currentMoveNum + 1;

				//Dedicating new memory to new instance of struct
				struct processNextMoveArgs* args = malloc(sizeof(struct processNextMoveArgs));
				args->board = newBoard;
				args->currentMoveNum = currentMoveNum + 1;
				args->currentXY[0] = futureMoves[i][0];
				args->currentXY[1] = futureMoves[i][1];
				args->squaresLocated = squaresLocated + 1;
				args->boardWidth = boardWidth;
				args->boardHeight = boardHeight;

				//Creates new thread
				pthread_create(&tid[i], NULL, &processNextMove, args);

				#ifdef NO_PARALLEL 
				void* returnValue = malloc(0);
				free(returnValue);
				pthread_join(tid[i], (void**) &returnValue);
				if (*(int*) returnValue > largest){
					largest = *(int*) returnValue;
				}	
				printf("THREAD %ld: Thread [%ld] joined (returned %d)\n", pthread_self(), tid[i], *(int*) returnValue);
				free(returnValue);
				#endif

			}
		}

		#ifndef NO_PARALLEL
			//Children must be processed to execute
			for (int i = 0; i < 8; i++){
				if (futureMoves[i][0] != 0 && futureMoves[i][1] != 0){
					void* returnValue = malloc(0); 
					free(returnValue);
					pthread_join(tid[i], (void**) &returnValue);		
					if (*(int*) returnValue > largest){
						largest = *(int*) returnValue;
					}			
					printf("THREAD %ld: Thread [%ld] joined (returned %d)\n", pthread_self(), tid[i], largest);
					free(returnValue);
				}
			}
	
		#endif

		//Frees memory
		freeFutureMoves(futureMoves);
		free(args1);
	}

	//If this is the main thread this branch is executed
	if (pthread_self() != mainThread){
		*returnSize = currentMoveNum;
		if (largest > *returnSize){
			*returnSize = largest;
		}
		//Freeing board
		freeBoardSpace(board, boardWidth);
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

	int boardWidth = atoi(argv[1]);
	int boardHeight = atoi(argv[2]);
	//Validate that m and n are greater than 2, if not send an error
	if (!(boardWidth > 2 && boardHeight > 2)){
		errorMessages();
		return EXIT_FAILURE;	
	}
	//If given x we store it, otherwise the value is 0. We also validate that it is not grater than the size of the board
	if (argc == 4) {
        x = atoi(argv[3]);
		if (x > boardWidth * boardHeight){
			errorMessages();
			return EXIT_FAILURE;	
		}
	} 
    else {
        x = 0;
    }

	//Creating the board dynamically as required 
	int** board = calloc(boardWidth, sizeof(int*));
	for (int i = 0; i < boardWidth; i++){
		board[i] = calloc(boardHeight, sizeof(int));
		for (int j = 0; j < boardHeight; j++){
			board[i][j] = 0;
		}
	}
	board[0][0] = 1; 

	//Setting up the global variables and dedicating new memory to new instance of struct
	max_squares = 0;
	dead_end_boards = calloc(10, sizeof(int**));
	dead_end_board_array_length = 10; 
	num_dead_end_boards = 0;
	struct processNextMoveArgs* args = malloc(sizeof(struct processNextMoveArgs));
	args->board = board;
	args->currentMoveNum = 1;
	args->currentXY[0] = 0;
	args->currentXY[1] = 0;
	args->squaresLocated = 1;
	args->boardWidth = boardWidth;
	args->boardHeight = boardHeight;

	//Creating the main thread, calling process args and printing results
	mainThread = pthread_self();
	processNextMove(args);
	printf("THREAD %ld: Best solution(s) found visit %d squares (out of %d)\n", pthread_self(), max_squares, boardHeight * boardWidth);

	//Print the dead end boards info
    printf("THREAD %ld: Dead end boards:\n", pthread_self());
	for (int i = 0; i < num_dead_end_boards; i++){
		printf("THREAD %ld: > ", pthread_self());
		for (int j = 0; j < boardWidth; j++){
			if (j != 0){
				printf("THREAD %ld:   ", pthread_self());
			}
			for (int k = 0; k < boardHeight; k++){
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
	for (int i = 0; i < num_dead_end_boards; i++){
		for (int j = 0; j < boardWidth; j++){
			free(dead_end_boards[i][j]);
		}
		free(dead_end_boards[i]);
	}
	free(dead_end_boards);
	pthread_mutex_destroy(&mutex);
	freeBoardSpace(board, boardWidth);

	return EXIT_SUCCESS;
}