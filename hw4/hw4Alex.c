#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>

// Global variables from hw4-main.c
extern int total_guesses;
extern int total_wins;
extern int total_losses;
extern char **words;

// Structure to hold arguments for handle_connection function
struct ThreadArgs {
    int client_socket;
    int seed;
    int num_words;
};

// Global variable to store thread arguments
struct ThreadArgs *global_thread_args;

// Function prototypes
void *handle_connection(void *arg);
void sigusr1_handler(int signum);

int wordle_server(int argc, char **argv) {
    // Check command-line arguments
    if (argc != 5) {
        fprintf(stderr, "ERROR: Invalid number of arguments\n");
        fprintf(stderr, "USAGE: hw4.out <listener-port> <seed> <word-filename> <num-words>\n");
        return EXIT_FAILURE;
    }

    // Set up signal handler for SIGUSR1
    struct sigaction sa;
    sa.sa_handler = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    // Set up TCP server
    int listener_port = atoi(*(argv + 1));
    int seed = atoi(*(argv + 2));
    char *word_filename = *(argv + 3);
    int num_words = atoi(*(argv + 4));


    // Initialize words array
    words = calloc(num_words + 1, sizeof(char *));
    if (words == NULL) {
        perror("calloc() action failed");
        return EXIT_FAILURE;
    }

    // Load words from file into words array
    FILE *file = fopen(word_filename, "r");
    if (file == NULL) {
        perror("filename not found");
        return EXIT_FAILURE;
    }

    char *buffer = malloc(1024 * sizeof(char));
    int word_count = 0;
    while (fgets(buffer, 1024, file) != NULL && word_count < num_words) {
        // Remove trailing newline character
        *(buffer + strcspn(buffer, "\n")) = '\0';

        // Allocate memory for the word and copy it into the words array
        *(words + word_count) = strdup(buffer);
        if (*(words + word_count) == NULL) {
            perror("word copying has failed");
            fclose(file);
            free(buffer);
            return EXIT_FAILURE;
        }

        word_count++;
    }
    free(buffer);

    fclose(file);

    // Set seed for pseudo-random number generator
    srand(seed);

    // Set up TCP socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket endpoint creation failed");
        return EXIT_FAILURE;
    }

    // Set SO_REUSEADDR option
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt() failed");
        close(server_socket); // Close the socket in case of error
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(listener_port);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("address assignment to socket failed");
        close(server_socket);
        return EXIT_FAILURE;
    }

    if (listen(server_socket, 5) == -1) {
        perror("listening to socket failed");
        close(server_socket);
        return EXIT_FAILURE;
    }

    printf("MAIN: opened ");
    char *ptr = word_filename;
    while (*ptr != '\0') {
        printf("%c", *ptr);
        ptr++;
    }
    printf(" (%d words)\n", num_words);
    printf("MAIN: Wordle server listening on port {%d}\n", listener_port);

    // Allocate memory for global thread arguments
    global_thread_args = malloc(sizeof(struct ThreadArgs));
    if (global_thread_args == NULL) {
        perror("malloc() action failed");
        return EXIT_FAILURE;
    }
    global_thread_args->seed = seed;
    global_thread_args->num_words = num_words;

    // Accept incoming connections
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket == -1) {
            perror("failed to accept socket");
            continue;
        }
        

        // Create a new instance of ThreadArgs and populate it with necessary values
        struct ThreadArgs *args = malloc(sizeof(struct ThreadArgs));
        args->client_socket = client_socket;
        args->seed = seed;
        args->num_words = num_words;

        // Handle connection in a new thread
        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_connection, args) != 0) {
            perror("failed to create thread");
            close(client_socket);
            free(args);
            continue;
        }


        printf("MAIN: rcvd incoming connection request\n");
    }

    // Close server socket
    close(server_socket);

    return EXIT_SUCCESS;
}

void sigusr1_handler(int signum) {
    // Your implementation of SIGUSR1 signal handler
    printf("MAIN: SIGUSR1 rcvd; Wordle server shutting down...\n");
    // Clean up global variables
    for (char **ptr = words; *ptr; ptr++)
        free(*ptr);
    free(words);
    //close(global_thread_args->client_socket);
    free(global_thread_args);
    //pthread_exit(NULL);
}

void *handle_connection(void *arg) {
    // Cast the argument back to ThreadArgs pointer
    struct ThreadArgs *args = (struct ThreadArgs *)arg;
    int client_socket = args->client_socket;
    int seed = args->seed;
    int num_words = args->num_words;
    // Free the memory allocated for the argument structure

    char *buffer = malloc(5 * sizeof(char));
    int n;
    int valid_guesses_left = 6;

    srand(seed);  // Use the provided seed

    // Generate a random index to select a word
    int random_index = rand() % num_words;
    char *hidden_word = words[random_index];
    printf("Proving the hidden word for testing purposes: %s\n", hidden_word);

    // Wait until a guess is received
    while (valid_guesses_left > 0) {
        // Print waiting message
        printf("THREAD (%ld): waiting for guess\n", pthread_self());
        //Attempts to read in data from socket
        if ((n = read(client_socket, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[n] = '\0';  // Null-terminate the received data

            // Check if the guess contains 5 lowercase or uppercase letters
            int valid_guess = 1;
            int valid_char_count = 0;
            for (int i = 0; buffer[i] != '\0'; i++) {
                if ((buffer[i] >= 'a' && buffer[i] <= 'z') || (buffer[i] >= 'A' && buffer[i] <= 'Z')) {
                    valid_char_count++;
                } else {
                    valid_guess = 0;
                    break;
                }
            }

            printf("THREAD (%ld): rcvd guess: %s\n", pthread_self(), buffer);

            if (valid_guess && valid_char_count == 5) {
                total_guesses++;
                valid_guesses_left--;
                // Loop through each character of the buffer
                
                // Iterate through each character in buffer
                for (int i = 0; i < strlen(buffer); i++) {
                    char current_char = buffer[i];
                    int found = 0;
                    // Iterate through each character in hidden_word
                    for (int j = 0; j < strlen(hidden_word); j++) {
                        // If the current buffer character matches a character in hidden_word
                        if (current_char == hidden_word[j] && j == i) {
                            buffer[i] = toupper(current_char);  // Replace buffer character with its uppercase version
                            found = 1;
                            break;
                        }
                        // If the current buffer character exists in hidden_word but at a different index
                        if (current_char == hidden_word[j] && j != i) {
                            buffer[i] = tolower(current_char);  // Replace buffer character with its lowercase version
                            found = 1;
                            break;
                        }
                    }
                    // If the current buffer character is not found in hidden_word at any index
                    if (found == 0) {
                        buffer[i] = '-';
                    }
                }
                printf("THREAD (%ld): sending reply: %s (%d guesses left)\n", pthread_self(), buffer, valid_guesses_left);
                //CHECKING FOR WIN CONDITION!
                char *hidden_word_uppercase = malloc((strlen(hidden_word) + 1) * sizeof(char));
                strcpy(hidden_word_uppercase, hidden_word);
                for (int i = 0; hidden_word_uppercase[i]; i++) {
                    hidden_word_uppercase[i] = toupper(hidden_word_uppercase[i]);
                }
                if (strcmp(buffer, hidden_word_uppercase) == 0) {
                    printf("THREAD (%ld): game over; word was %s!\n", pthread_self(), buffer);
                    sigusr1_handler(SIGUSR1);
                    pthread_exit(NULL);
                    printf("TESTING\n");
                }
            } else {
                strncpy(buffer, "?????", sizeof(buffer) - 1);
                buffer[sizeof(buffer) - 1] = '\0';
                // If the guess is not valid, send "?????"
                printf("THREAD (%ld): invalid guess; sending reply: %s (%d guesses left)\n", pthread_self(), buffer, valid_guesses_left);
            }

            // Clear the buffer for the next iteration
            memset(buffer, 0, 5 * sizeof(char));
        }
        else if (n == 0) {
            printf("THREAD (%ld): Client closed the connection\n", pthread_self());
            break; // Exit the loop if the client has closed the connection
        } 
        else {
            perror("Failed to read input from client");
            break; // Exit the loop on error
        }
    }
    sigusr1_handler(SIGUSR1);
    pthread_exit(NULL);
}
