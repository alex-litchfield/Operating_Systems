#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

//Function that gives the hash number for a word
int hashIndexer(char* curr_word, int cache_size) {
    int word_ascii_val = 0;
    for (int i = 0; i < strlen(curr_word); i++) {
        word_ascii_val = word_ascii_val + (int)(*(curr_word+i));
    }
    return word_ascii_val % cache_size;
}

int main(int argc, char** argv) {
    //If the number of command line arguments is not 3, the program is terminated
    if (argc != 3) {
        perror("Error: Please follow the correct usage: ./(program exe) (cache size) (text file)\n");
        exit(-1);
    }

    FILE* infile = fopen(*(argv+2), "r");
    int cache_size = atoi(*(argv+1));

    //If the cache size is not an integer when entered as a command line argument, the program is terminated
    if (cache_size == 0 && *(argv+1) != 0){
		perror("Error: The cache size argument is invalid\n");
		exit(-1);
	}

    //If the file does not exist, the program is terminated
    if (infile == NULL) {
        perror("Error: Failed to open file.\n");
        exit(-1);
    }

    //Initializing hash map and other variables
    char** hash_map = (char**)calloc(cache_size, sizeof(char*));
    char* curr_word = (char*)calloc(128 , sizeof(char));
    char curr_char;
    int char_counter = 0;

    //While loop that runs until every character from the read in file is read
    do {
        curr_char = fgetc(infile);
        //If the end of the file is reached, the loop ends
        if (feof(infile)) {
            break;
        }
        //If the current char IS alnum 
        if (isalnum(curr_char)) {
            *(curr_word + char_counter) = curr_char;
            char_counter = char_counter + 1;
        }
        //If the current char IS NOT alnum, append curr_char to curr_word, increment curr_char by 1
        else {
            if (char_counter >=3) {
                int word_ascii_val = hashIndexer(curr_word, cache_size);

                // If there is no word in the hash at the current position, add one
				if (*(hash_map + word_ascii_val) == 0){
					printf("Word \"%s\" ==> %d (calloc)\n", curr_word, word_ascii_val);
					*(hash_map + word_ascii_val) = calloc(strlen(curr_word) + 1, sizeof(char));
				}

				// If there is a word in the hash at the current position, replace it
				else {
					printf("Word \"%s\" ==> %d (realloc)\n", curr_word, word_ascii_val);
					*(hash_map + word_ascii_val) = (char*) realloc(*(hash_map + word_ascii_val), strlen(curr_word) + 1);
				}
                strcpy(*(hash_map + word_ascii_val), curr_word);
            }
            memset(curr_word, 0, 128);
            char_counter = 0;
        }
    } while(1);
    
    //Printing hash map, freeing memory, and closing the file
    for (int i = 0; i < cache_size; i++) {
        if (*(hash_map + i) != NULL) {
            printf("Cache index %d ==> \"%s\"\n", i, *(hash_map + i));
        }
        free(*(hash_map + i));
    }
    free(curr_word);
    free(hash_map);
    fclose(infile);
    return 0;
}