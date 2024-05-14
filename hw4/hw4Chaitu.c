#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
extern int total_guesses;
extern int total_wins;
extern int total_losses;
extern char ** wordList;
char* dictionaryFile;
int wordCount;
char* wordsPlayed;
int playedWordCount;
int shutdownSignal = 1;
int stopServer = 0;

typedef struct {
    int socketDescriptor;
    char* word;
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

char* fetchNewWord() {
    int n = wordCount;
    FILE *file = fopen(dictionaryFile, "r");
    if (!file) {
        perror("Failed to open dictionary file");
        return NULL;
    }
    char* word = calloc(6, sizeof(char));
    if (!word) {
        perror("Memory allocation failed for word");
        fclose(file);
        return NULL;
    }
    int index = rand() % n;
    fseek(file, 6 * index, SEEK_SET);
    fgets(word, 6, file);
    *(word + 5) = '\0';
    fclose(file);
    return word;
}

// Verify if the guessed word is correct
char* verifyWord(char* correctWord, char* guessedWord, int* isCorrect) {
    *isCorrect = 1; // Assume the guess is correct initially
    for (int i = 0; i < 5; i++) {
        char currentGuess = *(guessedWord + i);
        int found = 0;
        for (int j = 0; j < 5; j++) {
            if (currentGuess == *(correctWord + j)) {
                if (i == j) {
                    *(guessedWord + i) = toupper(currentGuess); // Correct position
                    found = 1;
                } else if (!found) {
                    // Correct character but wrong position
                    found = 2;
                }
            }
        }
        if (found == 0) {
            *(guessedWord + i) = '-'; // Not found at all
            *isCorrect = 0; // Set to false as one letter is wrong
        } else if (found == 2) {
            *(guessedWord + i) = tolower(currentGuess); // Right letter, wrong place
            *isCorrect = 0; // Set to false as position is wrong
        }
    }
    return guessedWord;
}


// Check if a string is a valid word in the dictionary
int isWord(char* word) {
    FILE *file = fopen(dictionaryFile, "r");
    if (!file) {
        perror("Failed to open dictionary file");
        return 0;
    }
    char* dictionaryWord = calloc(6, sizeof(char));
    if (!dictionaryWord) {
        perror("Memory allocation failed for dictionaryWord");
        fclose(file);
        return 0;
    }
    *(word + 5) = '\0';

    for (int i = 0; i < wordCount; i++) {
        fseek(file, 6 * i, SEEK_SET);
        fgets(dictionaryWord, 6, file);
        if (strncmp(dictionaryWord, word, 5) == 0) {
            fclose(file);
            free(dictionaryWord);
            return 1; // Word matches
        }
    }
    fclose(file);
    free(dictionaryWord);
    return 0;
}

void processWordGuess(int clientSocket, char* correctWord) {
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

            if (isWord(guess)) {
                remainingGuesses--;
                pthread_mutex_lock(&mutex);
                guess = verifyWord(correctWord, guess, isCorrect);
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

    free(buffer);
    free(isCorrect);
    free(correctWord);
    close(clientSocket);
}

void* threadFunction(void * arg) {
    ThreadArgs* threadArgs = (ThreadArgs*) arg;

    processWordGuess(threadArgs->socketDescriptor, threadArgs->word);
    pthread_exit(NULL);
}

int wordle_server(int argc, char ** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc != 5) {
        perror("Error: Invalid argument(s)\nUSAGE: hw4.out <listener-port> <seed> <word-filename> <num-words>\n");
        return EXIT_FAILURE;
    }
    int port = atoi(*(argv + 1));
	unsigned int seed=(unsigned int)atoi(*(argv+2));
	dictionaryFile = *(argv+3);
	wordCount = atoi(*(argv+4));

    srand(seed);

    printf("MAIN: opened %s (%d words)\n", dictionaryFile, wordCount);
    printf("MAIN: Wordle server listening on port {%d}\n", port);

    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == -1) {
        perror("socket() failed");
        return EXIT_FAILURE;
    }

    struct sockaddr_in tcpServer;
    tcpServer.sin_family = AF_INET;
    tcpServer.sin_addr.s_addr = htonl(INADDR_ANY);
    tcpServer.sin_port = htons(port);

    if (bind(listener, (struct sockaddr *)&tcpServer, sizeof(tcpServer)) == -1) {
        perror("bind() failed");
        return EXIT_FAILURE;
    }

    if (listen(listener, 5) == -1) {
        perror("listen() failed");
        return EXIT_FAILURE;
    }

    wordsPlayed = calloc(50, sizeof(char));
    if (!wordsPlayed) {
        perror("Memory allocation failed for wordsPlayed");
        return EXIT_FAILURE;
    }
    playedWordCount = 0;

    while (!stopServer) {
        struct sockaddr_in remoteClient;
        int addrlen = sizeof(remoteClient);

        int clientSocket = accept(listener, (struct sockaddr *)&remoteClient, (socklen_t *)&addrlen);
        if (clientSocket == -1) {
            perror("accept() failed");
            continue;
        }

        printf("MAIN: rcvd incoming connection request\n");

        pthread_mutex_lock(&mutex);
        char* word = fetchNewWord();
        for (int i = 0; i < 5; i++) {
            *(wordsPlayed + playedWordCount) = *(word + i);
            playedWordCount++;
        }

        pthread_mutex_unlock(&mutex);

        ThreadArgs* threadArgs = calloc(1, sizeof(ThreadArgs));
        if (!threadArgs) {
            perror("Memory allocation failed for threadArgs");
            free(word);
            continue;
        }
        threadArgs->socketDescriptor = clientSocket;
        threadArgs->word = word;

        pthread_t threadId;
        pthread_create(&threadId, NULL, threadFunction, threadArgs);
    }
    printf("Wordle server shutting down...\n");

    close(listener);

    return EXIT_SUCCESS;
}