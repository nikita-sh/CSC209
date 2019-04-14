#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_state(char *state, int size) {
    for (int i=0; i<size; i++) {
        printf("%c", state[i]);
    }
    printf("\n");
}

void update_state(char *state, int size) {
    char copy[size];
    strncpy(copy, state, size);
    for (int i=1; i<size-1; i++) {
        if ((copy[i-1] == '.' && copy[i+1] == '.') || (copy[i-1] == 'X' && copy[i+1] == 'X')) {
            state[i] = '.';
        } else {
            state[i] = 'X';
        }
    }
}