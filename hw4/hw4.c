#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

//Required global viarables
extern int total_guesses;
extern int total_wins;
extern int total_losses;
extern char ** wordList;

//Mutex
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

//Additional global variables
char* playedWords;
int killServer = 0;
int killSignal = 1;
int playedWordCounter;

//Defining Struct
typedef struct {
    char* hiddenWord;
    char* dictFile;
    int numWords;
    int sd;
} ThreadArgs;

//Signal handler - ignores all signals except SIGUSR1 and shuts down server
void handleSignal(int sigNum) {
    printf("MAIN: SIGUSR1 rcvd; Wordle server shutting down...\n");
    pthread_mutex_lock(&mutex);
    killServer = 1;
    pthread_mutex_unlock(&mutex);
    printf("MAIN: Wordle server shutting down...\n");
    printf("\nMAIN: guesses: %d\nMAIN: wins: %d\nMAIN: losses: %d\n\n", total_guesses, total_wins, total_losses);

    printf("MAIN: word(s) played: ");
    for (int i = 1; i <= playedWordCounter; i++) {
        printf("%c", toupper(*(playedWords + i - 1)));
        if (i % 5 == 0 && i < playedWordCounter - 1) {
            printf(" ");
        }
    }
    printf("\n");

    free(playedWords);
}

//Handles the connection and game of wordle
void* threadFunction(void * arg) {
    //Setting up struct variables in easy to access manner
    ThreadArgs* threadArgs = (ThreadArgs*) arg;
    char* rightWord = threadArgs->hiddenWord;
    char* dictFile = threadArgs->dictFile;
    int numWords = threadArgs->numWords;
    int cs = threadArgs->sd;

    //If a shutdown signal is present, ignore it unless it is of SIGUSR1
    //Then proceed to shut down the server
    if (killSignal) {
        signal(SIGUSR2, SIG_IGN);
        signal(SIGTERM, SIG_IGN);
        signal(SIGINT, SIG_IGN);
        signal(SIGUSR1, handleSignal);
    }

    pthread_detach(pthread_self());

    //Allocating memory and setting up variables
    int guessesLeft = 6;
    int* correctMarker = calloc(1, sizeof(int));
    if (correctMarker == NULL) {
        fprintf(stderr, "calloc() failed - correctMarker()");
        pthread_exit(NULL);
    }
    int grabbedBytes;
    char *buffer = calloc(9, sizeof(char));
    if (buffer == NULL) {
        free(correctMarker);
        fprintf(stderr, "calloc() failed - buffer");
        pthread_exit(NULL);
    }
    while (guessesLeft > 0 && killServer == 0) {
        printf("THREAD %lu: waiting for guess\n", pthread_self());
        grabbedBytes = recv(cs, buffer, 16, 0);
        if (grabbedBytes == 0) {
            printf("THREAD %lu: client gave up; closing TCP connection...\n", pthread_self());
            pthread_mutex_lock(&mutex);
            total_losses++;
            pthread_mutex_unlock(&mutex);
            break;
        }

        if (grabbedBytes == -1) {
            free(correctMarker);
            free(buffer);
            fprintf(stderr, "Failed to receive bytes");
            pthread_exit(NULL);
        } else {
            FILE *file = fopen(dictFile, "r");
            int correctGuessMarker = 0;
            *(buffer + grabbedBytes) = '\0';
            printf("THREAD %lu: rcvd guess: %s\n", pthread_self(), buffer);

            char* currGuess = calloc(6, sizeof(char));
            if (currGuess == NULL) {
                free(correctMarker);
                free(buffer);
                fprintf(stderr, "calloc() failed - currGuess");
                pthread_exit(NULL);
            }
            strncpy(currGuess, buffer, 5);

            if (file == NULL) {
                fprintf(stderr, "Couldn't open the dictionary");
            }
            char* dictWord = calloc(6, sizeof(char));
            if (!dictWord) {
                fprintf(stderr, "calloc() failed - dictWord");
                correctGuessMarker = 0;
                fclose(file);
                break;
            }
            *(currGuess + 5) = '\0';
            for (int i = 0; i < numWords; i++) {
                fseek(file, 6 * i, SEEK_SET);
                fgets(dictWord, 6, file);
                if (strncmp(dictWord, currGuess, 5) == 0) {
                    correctGuessMarker = 1;
                    break;
                }
            }
            fclose(file);
            free(dictWord);

            //If guessed word was correct
            if (correctGuessMarker == 1) {
                guessesLeft--;
                pthread_mutex_lock(&mutex);
                *correctMarker = 1;
                for (int i = 0; i < 5; i++) {
                    int wordFound = 0;
                    for (int j = 0; j < 5; j++) {
                        if (*(currGuess + i) == *(rightWord + j)) {
                            if (i == j) {
                                wordFound = 1;
                                *(currGuess + i) = toupper(*(currGuess + i));
                            } 
                            else if (wordFound == 0) {
                                wordFound = 2;
                            }
                        }
                    }
                    //One letter is wrong
                    if (wordFound == 0) {
                        *correctMarker = 0;
                        *(currGuess + i) = '-';
                    //Letter is correct but in wrong location
                    } else if (wordFound == 2) {
                        *(currGuess + i) = tolower(*(currGuess + i));
                        *correctMarker = 0;
                    }
                }
                total_guesses++;
                *(buffer) = 'Y';
                *(int*)(buffer + 1) = htons(guessesLeft);
                strncpy(buffer + 3, currGuess, 5);
                printf("THREAD %lu: sending reply: %s (%d guess", pthread_self(), currGuess, guessesLeft);

            } else {
                *(buffer) = 'N';
                *(int*)(buffer + 1) = htons(guessesLeft);
                memset(buffer + 3, '?', 5);
                printf("THREAD %lu: invalid guess; sending reply: ????? (%d guess", pthread_self(), guessesLeft);
            }
            if (guessesLeft != 1) {
                printf("es");
            }
            printf(" left)\n");

            grabbedBytes = send(cs, buffer, 9, 0);
            if (*correctMarker != 0) {
                total_wins++;
                pthread_mutex_unlock(&mutex);
                break;
            }
            if (grabbedBytes == -1) {
                free(correctMarker);
                free(buffer);
                free(currGuess);
                pthread_exit(NULL);
                fprintf(stderr, "Failed to send data");
            }
            pthread_mutex_unlock(&mutex);
        }
    }

    if (guessesLeft == 0) {
        pthread_mutex_lock(&mutex);
        total_losses++;
        pthread_mutex_unlock(&mutex);
    }

    printf("THREAD %lu: game over; word was ", pthread_self());
    for (int i = 0; i < 5; i++) {
        printf("%c", toupper(*(rightWord + i)));
    }
    printf("!\n");

    free(correctMarker);
    free(rightWord);
    free(buffer);
    close(cs);
    pthread_exit(NULL);
}

//Creates the wordle server
int wordle_server(int argc, char ** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);

    //Invalid number of arguments
    if (argc != 5) {
        fprintf(stderr, "ERROR: Invalid number of arguments\n");
        fprintf(stderr, "USAGE: hw4.out <listener-port> <seed> <word-filename> <num-words>\n");
        return EXIT_FAILURE;
    }

    //Bringing in arguments as variables
    int listeningPort = atoi(*(argv + 1));
	int seed = atoi(*(argv+2)); 
	char* dictFile = *(argv+3);
	int numWords = atoi(*(argv+4));

    //Srand as required
    srand(seed);

    //Listening to the socket
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == -1) {
        fprintf(stderr, "socket endpoint creation failed\n");
        return EXIT_FAILURE;
    }

    //Setting up TCP server
    struct sockaddr_in TCP;
    TCP.sin_family = AF_INET;
    TCP.sin_addr.s_addr = htonl(INADDR_ANY);
    TCP.sin_port = htons(listeningPort);

    //Binding listener socket to the server
    if (bind(listenSocket, (struct sockaddr *)&TCP, sizeof(TCP)) == -1) {
        fprintf(stderr, "binding address assignment to socket failed\n");
        return EXIT_FAILURE;
    }

    //Allowing server to listen
    if (listen(listenSocket, 5) == -1) {
        fprintf(stderr, "listening to socket failed\n");
        return EXIT_FAILURE;
    }
    
    playedWords = calloc(100, sizeof(char));
    playedWordCounter = 0;
    printf("MAIN: opened %s (%d words)\n", dictFile, numWords);
    printf("MAIN: Wordle server listening on port {%d}\n", listeningPort);

    //While we want the server to keep going, we loop
    while (!killServer) {
        struct sockaddr_in remoteClient;
        socklen_t addrlen = sizeof(remoteClient); 
        int cs = accept(listenSocket, (struct sockaddr *)&remoteClient, (socklen_t *)&addrlen);
        if (cs == -1) {
            fprintf(stderr, "failed to accept socket");
            continue;
        }
        printf("MAIN: rcvd incoming connection request\n");
        pthread_mutex_lock(&mutex);
        //Checking file validity
        FILE *file = fopen(dictFile, "r");
        if (file == NULL) {
            fprintf(stderr, "Failed to open dictionary file");
            return EXIT_FAILURE;
        }
        //Checking for calloc ability
        char* hiddenWord = calloc(6, sizeof(char));
        if (hiddenWord == NULL) {
            fclose(file);
            return EXIT_FAILURE;
        }
        else {
            int index = rand() % numWords;
            fseek(file, 6 * index, SEEK_SET);
            fgets(hiddenWord, 6, file);
            *(hiddenWord + 5) = '\0';
            fclose(file);
        }
        for (int i = 0; i < 5; i++) {
            *(playedWords + playedWordCounter) = *(hiddenWord + i);
            playedWordCounter++;
        }
        pthread_mutex_unlock(&mutex);

        //Creating struct for thread and allocating space
        ThreadArgs* threadArgs = calloc(1, sizeof(ThreadArgs));
        if (threadArgs == NULL) {
            free(hiddenWord);
            continue;
        }
        threadArgs->hiddenWord = hiddenWord;
        threadArgs->dictFile = dictFile;
        threadArgs->numWords = numWords;
        threadArgs->sd = cs;

        //Creating thread
        pthread_t threadId;
        pthread_create(&threadId, NULL, threadFunction, threadArgs);
    }
    printf("Wordle server shutting down...\n");
    close(listenSocket);
    return EXIT_SUCCESS;
}