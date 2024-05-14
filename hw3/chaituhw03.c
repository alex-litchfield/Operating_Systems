#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

struct threadArgs {
    int width;
	int height;
	int** board;
	int current;
	int visited;
	int position[2];
};

// Global Variables
int x; // Threshold for considering a board as a dead end
pthread_t mainT; // Main thread
int dead_end_size; 
int dead_end_num;
int*** dead_end; // Collection of dead end boards
int max_visited; // Maximum number of squares visited in a solution

// Mutex
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Function to calculate potential moves for the knight
void searchPotentialMoves(int** board, int* position, int width, int height, int* numMoves, int** nextMoves) {
    // Define possible moves offsets
    int rowOffset[] = {-1, -2, -2, -1, 1, 2, 2, 1};
    int colOffset[] = {-2, -1, 1, 2, 2, 1, -1, -2};
    
    // Iterate through possible moves
    for (int i = 0; i < 8; ++i) {
        int newRow = position[0] + rowOffset[i];
        int newCol = position[1] + colOffset[i];
        
        // Check if the new position is valid and empty
        if (newRow >= 0 && newRow < width && newCol >= 0 && newCol < height && board[newRow][newCol] == 0) {
            nextMoves[i][0] = newRow;
            nextMoves[i][1] = newCol;
            (*numMoves)++;
        } else {
            nextMoves[i][0] = 0;
            nextMoves[i][1] = 0;
        }
    }
}

// Function to add a dead end board to the collection
void addDeadEndBoard(int width, int height, int** board, int visited){
	// If this is not a valid dead_end_board, free it 
	if (visited < x){
		for (int i = 0; i < width; i++){
			free(board[i]);
		}
		free(board);
		return; 
	}
	pthread_mutex_lock(&mutex);
	// If the dead_end capacity is filled
	if (dead_end_num + 1 >= dead_end_size){
		dead_end = realloc(dead_end, (dead_end_size + 10) * sizeof(int***));
		dead_end_size += 10;
	}
	// Add the board to the dead_end
	dead_end[dead_end_num] = board; 
	dead_end_num += 1;
	pthread_mutex_unlock(&mutex);
	return; 
}


// Function to output all dead end boards and free memory
void printAndFreeDeadEndBoards(int width, int height){
	printf("THREAD %ld: Dead end boards:\n", pthread_self());
	for (int i = 0; i < dead_end_num; i++){
		printf("THREAD %ld: > ", pthread_self());
		for (int j = 0; j < width; j++){
			if (j != 0){
				printf("THREAD %ld:   ", pthread_self());
			}
			for (int k = 0; k < height; k++){
				if (dead_end[i][j][k] == 0){
					printf(".");
				} else {
					printf("S");
				}
			}
		printf("\n");
		}
	}
	for (int i = 0; i < dead_end_num; i++){
		for (int j = 0; j < width; j++){
			free(dead_end[i][j]);
		}
		free(dead_end[i]);
	}
	free(dead_end);
	return;
}
// Function to process the next move in the knight's tour
void* processNextMove(void* arguments){
    struct threadArgs* threadArgs = arguments;

    // Extract arguments
	int width = threadArgs->width;
    int height = threadArgs->height;
    int** board = threadArgs->board;
    int current = threadArgs->current;
    int visited = threadArgs->visited;
    int position[2];
    position[0] = threadArgs->position[0];
    position[1] = threadArgs->position[1];
    
    if (current == 1){
		printf("THREAD %ld: Solving Sonny's knight's tour problem for a %dx%d board\n", pthread_self(), width, height);
	}

    int largest = current;

    int* ret = malloc(1*sizeof(int));
	*ret = 0;

    int numMoves = 0; 

	int** nextMoves = calloc(8, sizeof(int*));
	for (int i = 0; i < 8; i++){
		nextMoves[i] = calloc(2, sizeof(int));
	}
    searchPotentialMoves(board, position, width, height, &numMoves, nextMoves);
    if (numMoves == 1){
		free(ret);
		free(threadArgs);
		// Find the next move, input it into the board and processNextMove 
		int nextPosition[2];
        for (int i = 0; i < 8; i++){
			if (nextMoves[i][0] != 0 || nextMoves[i][1] != 0){
				nextPosition[0] = nextMoves[i][0];
				nextPosition[1] = nextMoves[i][1];
                board[nextPosition[0]][nextPosition[1]] = current + 1;
				break;
			}
		}
		for (int i = 0; i < 8; i++){
			free(nextMoves[i]);
		}
		free(nextMoves);
        struct threadArgs* args = malloc(sizeof(struct threadArgs));
		args->width = width;
		args->height = height;
		args->board = board;
		args->current = current + 1;
		args->visited = visited + 1;
		args->position[0] = nextPosition[0];
		args->position[1] = nextPosition[1];
		processNextMove(args);
	}
    else if( numMoves > 1){
        free(threadArgs);
        pthread_t thread_id[8];
		printf("THREAD %ld: %d moves possible after move #%d; creating threads...\n", pthread_self(), numMoves, current);
        for (int i = 0; i < 8; i++){
			if (nextMoves[i][0] != 0 || nextMoves[i][1] != 0){
				// Create a new board for this specific move 
				int** newBoard = calloc(width, sizeof(int*));
				for (int j = 0; j < width; j++){
					newBoard[j] = calloc(height, sizeof(int));
					for (int k = 0; k < height; k++){
						newBoard[j][k] = board[j][k];
					}
				}
				newBoard[nextMoves[i][0]][nextMoves[i][1]] = current + 1;
				int position_new[2];
				position_new[0] = nextMoves[i][0];
				position_new[1] = nextMoves[i][1];

				struct threadArgs* args = malloc(sizeof(struct threadArgs));
				args->width = width;
				args->height = height;
				args->board = newBoard;
				args->current = current + 1;
				args->visited = visited + 1;
				args->position[0] = position_new[0];
				args->position[1] = position_new[1];

				pthread_create(&thread_id[i], NULL, &processNextMove, args);
				#ifdef NO_PARALLEL 
				void* returnValue = malloc(0);
				free(returnValue);

				pthread_join(thread_id[i], (void**) &returnValue);

				if (*(int*) returnValue > largest){
					largest = *(int*) returnValue;
				}	

				printf("THREAD %ld: Thread [%ld] joined (returned %d)\n", pthread_self(), thread_id[i], *(int*) returnValue);
				free(returnValue);
				#endif
			}
		}
        #ifndef NO_PARALLEL
			// Wait for all children to be processed
			for (int i = 0; i < 8; i++){
				if (nextMoves[i][0] != 0 && nextMoves[i][1] != 0){
					
					void* returnValue = malloc(0); 
					free(returnValue);

					pthread_join(thread_id[i], (void**) &returnValue);		
					
					if (*(int*) returnValue > largest){
						largest = *(int*) returnValue;
					}			

					printf("THREAD %ld: Thread [%ld] joined (returned %d)\n", pthread_self(), thread_id[i], largest);
					free(returnValue);
				}
			}
		#endif
		for (int i = 0; i < 8; i++){
			free(nextMoves[i]);
		}
		free(nextMoves);
    }
    else if(numMoves == 0){
        if (visited == width * height){
			printf("THREAD %ld: Sonny found a full knight's tour!\n", pthread_self());
			// Free the board 
			for (int i = 0; i < width; i++){
				free(board[i]);
			}
			free(board);
		}else if (visited < width * height){
			printf("THREAD %ld: Dead end after move #%d\n", pthread_self(), current);
			addDeadEndBoard(width, height, board, visited);
		}
        if (current > max_visited){
			max_visited = current;
		}
        for (int i = 0; i < 8; i++){
			free(nextMoves[i]);
		}
		free(nextMoves);
		*ret = current;
		pthread_exit(ret);
    }
    if (pthread_self() != mainT){

		*ret = current;
		if (largest > *ret){
			*ret = largest;
		}

		for (int i = 0; i < width; i++){
			free(board[i]);
		}
		free(board);

		pthread_exit(ret);
	}
	free(ret);
	return ret;

}



int main(int argc, char** argv){
    // Disable buffered output for stdout 
	setvbuf( stdout, NULL, _IONBF, 0 );

    if (argc < 3){
		fprintf(stderr, "ERROR: Invalid argument(s)\n");
		fprintf(stderr, "USAGE: a.out <m> <n> [<x>]\n");
		return EXIT_FAILURE;	
	}

    int width = atoi(argv[1]);
	int height = atoi(argv[2]);
	int positionStart[2];

    // Check if m and n are not less than ore equal 2
	if ((width <= 2 || height <= 2)){
		fprintf(stderr, "ERROR: Invalid argument(s)\n");
		fprintf(stderr, "USAGE: a.out <m> <n> [<x>]\n");
		return EXIT_FAILURE;	
	}

    if (argc == 4){
        int sum = width * height;
		x = atoi(argv[3]);
		if (x > sum){
			fprintf(stderr, "ERROR: Invalid argument(s)\n");
			fprintf(stderr, "USAGE: a.out <m> <n> [<x>]\n");
			return EXIT_FAILURE;	
		}
	}
	else {
		x = 0;
	}

    // Global Variables
	max_visited = 0;
	dead_end = calloc(10, sizeof(int**));
	dead_end_size = 10; 
	dead_end_num = 0;
    max_visited = 0;

    // Initialize the starting board 
	int** board = calloc(width, sizeof(int*));
	for (int i = 0; i < width; i++){
		board[i] = calloc(height, sizeof(int));
		for (int j = 0; j < height; j++){
			board[i][j] = 0;
		}
	}
	
    board[0][0] = 1;   
	positionStart[0] = 0;
	positionStart[1] = 0;
    mainT = pthread_self();

    struct threadArgs* arguments = malloc(sizeof(struct threadArgs));

	arguments->board = board;
	arguments->width = width;
	arguments->height = height;
	arguments->current = 1;
	arguments->position[0] = positionStart[0];
	arguments->position[1] = positionStart[1];
	arguments->visited = 1;

    processNextMove(arguments);

	printf("THREAD %ld: Best solution(s) found visit %d squares (out of %d)\n", pthread_self(), max_visited, width * height);
    
    printAndFreeDeadEndBoards(width, height);

	// End Mutex
	pthread_mutex_destroy(&mutex);
    for (int i = 0; i < width; i++){
		free(board[i]);
	}
	free(board);
	return EXIT_SUCCESS;
}