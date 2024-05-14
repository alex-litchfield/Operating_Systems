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
char* wordsPlayed;
int playedWordCount;
int shutdownSignal = 1;
int stopServer = 0;

typedef struct {
    int socketDescriptor;
    char* word;
    int wordCount;
    char* dictionaryFile;
} ThreadArgs;


void handleSignal(int signalNumber) {
    printf("MAIN: SIGUSR1 rcvd; Wordle server shutting down...\n");
    pthread_mutex_lock(&mutex);
    stopServer = 1;
    pthread_mutex_unlock(&mutex);
    printf("MAIN: Wordle server shutting down...\n");
    printf("\nMAIN: guesses: %d\nMAIN: wins: %d\nMAIN: losses: %d\n\n", total_guesses, total_wins, total_losses);

    printf("MAIN: word(s) played: ");
    for (int i = 1; i <= playedWordCount; i++) {
        printf("%c", toupper(*(wordsPlayed + i - 1)));
        if (i % 5 == 0 && i < playedWordCount - 1) {
            printf(" ");
        }
    }
    printf("\n");

    free(wordsPlayed);
}

void* threadFunction(void * arg) {
    ThreadArgs* threadArgs = (ThreadArgs*) arg;
    int clientSocket = threadArgs->socketDescriptor;
    char* correctWord = threadArgs->word;
    int wordCount = threadArgs->wordCount;
    char* dictionaryFile = thread->dictionaryFile;
    if (shutdownSignal) {
        signal(SIGINT, SIG_IGN);
        signal(SIGTERM, SIG_IGN);
        signal(SIGUSR2, SIG_IGN);
        signal(SIGUSR1, handleSignal);
    }

    pthread_t myThreadId = pthread_self();
    pthread_detach(myThreadId);

    unsigned short remainingGuesses = 6;
    int* isCorrect = calloc(1, sizeof(int));
    if (!isCorrect) {
        perror("Memory allocation failed for isCorrect");
        pthread_exit(NULL);
    }
    int receivedBytes;
    char *buffer = calloc(9, sizeof(char));
    if (!buffer) {
        perror("Memory allocation failed for buffer");
        free(isCorrect);
        pthread_exit(NULL);
    }
    do {
        printf("THREAD %ld: waiting for guess\n", pthread_self());
        receivedBytes = recv(clientSocket, buffer, 16, 0);
        if (receivedBytes == 0) {
            printf("THREAD %ld: client gave up; closing TCP connection...\n", pthread_self());
            pthread_mutex_lock(&mutex);
            total_losses++;
            pthread_mutex_unlock(&mutex);
            break;
        }

        if (receivedBytes == -1) {
            perror("recv() failed");
            free(buffer);
            free(isCorrect);
            pthread_exit(NULL);
        } else {
            *(buffer + receivedBytes) = '\0';
            printf("THREAD %ld: rcvd guess: %s\n", pthread_self(), buffer);

            char* guess = calloc(6, sizeof(char));
            if (!guess) {
                perror("Memory allocation failed for guess");
                free(buffer);
                free(isCorrect);
                pthread_exit(NULL);
            }
            strncpy(guess, buffer, 5);

            FILE *file = fopen(dictionaryFile, "r");
            int guessCorrect = 0;
            if (!file) {
                fprintf(stderr, "Couldn't open the dictionary");
            }
            char* dictionaryWord = calloc(6, sizeof(char));
            if (file && dictionaryWord) {
                *(guess + 5) = '\0';
                for (int i = 0; i < wordCount; i++) {
                    fseek(file, 6 * i, SEEK_SET);
                    fgets(dictionaryWord, 6, file);
                    if (strncmp(dictionaryWord, guess, 5) == 0) {
                        guessCorrect = 1;
                        break;
                    }
                }
            }
            fclose(file);
            free(dictionaryWord);

            if (guessCorrect == 1) {
                remainingGuesses--;
                pthread_mutex_lock(&mutex);

                *isCorrect = 1; // Assume the guess is correct initially
                for (int i = 0; i < 5; i++) {
                    char currentGuess = *(guess + i);
                    int found = 0;
                    for (int j = 0; j < 5; j++) {
                        if (currentGuess == *(correctWord + j)) {
                            if (i == j) {
                                *(guess + i) = toupper(currentGuess); // Correct position
                                found = 1;
                            } else if (!found) {
                                // Correct character but wrong position
                                found = 2;
                            }
                        }
                    }
                    if (found == 0) {
                        *(guess + i) = '-'; // Not found at all
                        *isCorrect = 0; // Set to false as one letter is wrong
                    } else if (found == 2) {
                        *(guess + i) = tolower(currentGuess); // Right letter, wrong place
                        *isCorrect = 0; // Set to false as position is wrong
                    }
                }



                *(buffer) = 'Y';
                *(short*)(buffer + 1) = htons(remainingGuesses);
                strncpy(buffer + 3, guess, 5);
                total_guesses++;
                printf("THREAD %ld: sending reply: ", pthread_self());
                printf("%s", guess);

            } else {
                *(buffer) = 'N';
                *(short*)(buffer + 1) = htons(remainingGuesses);
                memset(buffer + 3, '?', 5);
                printf("THREAD %ld: invalid guess; sending reply: ?????", pthread_self());
            }

            printf(" (%d guess", remainingGuesses);
            if (remainingGuesses != 1) printf("es");
            printf(" left)\n");

            receivedBytes = send(clientSocket, buffer, 9, 0);
            if (*isCorrect) {
                total_wins++;
                pthread_mutex_unlock(&mutex);
                break;
            }
            if (receivedBytes == -1) {
                perror("send() failed");
                free(guess);
                free(buffer);
                free(isCorrect);
                free(dictionaryFile);
                pthread_exit(NULL);
            }
            pthread_mutex_unlock(&mutex);
        }
    } while (remainingGuesses > 0 && stopServer == 0);

    if (remainingGuesses == 0) {
        pthread_mutex_lock(&mutex);
        total_losses++;
        pthread_mutex_unlock(&mutex);
    }

    printf("MAIN: game over; word was ");
    for (int i = 0; i < 5; i++) {
        printf("%c", toupper(*(correctWord + i)));
    }
    printf("!\n");

    free(dictionaryFile);
    free(buffer);
    free(isCorrect);
    free(correctWord);
    close(clientSocket);
    pthread_exit(NULL);
}

int wordle_server(int argc, char ** argv) {
    setvbuf(stdout, NULL, _IONBF, 0); //IS THIS NEEDED?
    if (argc != 5) {
        fprintf(stderr, "ERROR: Invalid number of arguments\n");
        fprintf(stderr, "USAGE: hw4.out <listener-port> <seed> <word-filename> <num-words>\n");
        return EXIT_FAILURE;
    }

    int port = atoi(*(argv + 1));
	unsigned int seed=(unsigned int)atoi(*(argv+2)); //lets see if we can make this an int
	char* dictionaryFile = *(argv+3);
	int wordCount = atoi(*(argv+4));

    srand(seed);

    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == -1) {
        fprintf(stderr, "socket endpoint creation failed\n");
        return EXIT_FAILURE;
    }

    struct sockaddr_in tcpServer;
    tcpServer.sin_family = AF_INET;
    tcpServer.sin_addr.s_addr = htonl(INADDR_ANY);
    tcpServer.sin_port = htons(port);

    if (bind(listener, (struct sockaddr *)&tcpServer, sizeof(tcpServer)) == -1) {
        fprintf(stderr, "binding address assignment to socket failed\n");
        return EXIT_FAILURE;
    }

    if (listen(listener, 5) == -1) {
        fprintf(stderr, "listening to socket failed\n");
        return EXIT_FAILURE;
    }
    
    //Allocating space for words Played
    wordsPlayed = calloc(100, sizeof(char));
    playedWordCount = 0;

    printf("MAIN: opened %s (%d words)\n", dictionaryFile, wordCount);
    printf("MAIN: Wordle server listening on port {%d}\n", port);

    //While stopServer is false
    while (!stopServer) {
        struct sockaddr_in remoteClient;
        socklen_t addrlen = sizeof(remoteClient); //Changed from int
        int clientSocket = accept(listener, (struct sockaddr *)&remoteClient, (socklen_t *)&addrlen);
        if (clientSocket == -1) {
            fprintf(stderr, "failed to accept socket");
            continue;
        }

        printf("MAIN: rcvd incoming connection request\n");

        pthread_mutex_lock(&mutex);
        
        FILE *file = fopen(dictionaryFile, "r");
        if (!file) {
            fprintf(stderr, "Failed to open dictionary file");
            return EXIT_FAILURE;
        }
        char* word = calloc(6, sizeof(char));
        if (!word) {
            fclose(file);
            return EXIT_FAILURE;
        }
        else {
            int index = rand() % wordCount;
            fseek(file, 6 * index, SEEK_SET);
            fgets(word, 6, file);
            *(word + 5) = '\0';
            fclose(file);
        }

        for (int i = 0; i < 5; i++) {
            *(wordsPlayed + playedWordCount) = *(word + i);
            playedWordCount++;
        }

        pthread_mutex_unlock(&mutex);

        ThreadArgs* threadArgs = calloc(1, sizeof(ThreadArgs));
        if (!threadArgs) {
            free(word);
            continue;
        }
        threadArgs->socketDescriptor = clientSocket;
        threadArgs->word = word;
        threadArgs->wordCount = wordCount;
        threadArgs->dictionaryFile = dictionaryFile;

        pthread_t threadId;
        pthread_create(&threadId, NULL, threadFunction, threadArgs);
    }
    printf("Wordle server shutting down...\n");

    close(listener);

    return EXIT_SUCCESS;
}