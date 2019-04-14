#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Constants that determine that address ranges of different memory regions

#define GLOBALS_START 0x400000
#define GLOBALS_END   0x700000
#define HEAP_START   0x4000000
#define HEAP_END     0x8000000
#define STACK_START 0xfff000000

int main(int argc, char **argv) {
    
    FILE *fp = NULL;

    if(argc == 1) {
        fp = stdin;

    } else if(argc == 2) {
        fp = fopen(argv[1], "r");
        if(fp == NULL){
            perror("fopen");
            exit(1);
        }
    } else {
        fprintf(stderr, "Usage: %s [tracefile]\n", argv[0]);
        exit(1);
    }

    unsigned long addr;
    char access;
    int icount, mcount, scount, lcount, global_count, heap_count, stack_count;
    icount = mcount = scount = lcount = global_count = heap_count = stack_count = 0;
    while (fscanf(fp, "%c,%lx\n", &access, &addr) != EOF) {
        if (access == 'I') {
            icount += 1;
        } else if (access == 'M') {
            mcount += 1;
        } else if (access == 'S') {
            scount += 1;
        } else {
            lcount += 1;
        }
        if (access != 'I') {
            if (HEAP_START < addr && addr < HEAP_END) {
                heap_count += 1;
            } else if (GLOBALS_START < addr && addr < GLOBALS_END) {
                global_count += 1;
            } else if (STACK_START < addr) {
                stack_count += 1;
            }
        }
    }
    printf("Reference Counts by Type:\n");
    printf("    Instructions: %d\n", icount);
    printf("    Modifications: %d\n", mcount);
    printf("    Loads: %d\n", lcount);
    printf("    Stores: %d\n", scount);
    printf("Data Reference Counts by Location:\n");
    printf("    Globals: %d\n", global_count);
    printf("    Heap: %d\n", heap_count);
    printf("    Stack: %d\n", stack_count);
    return 0;
}
