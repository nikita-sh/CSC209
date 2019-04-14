#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Reads a trace file produced by valgrind and an address marker file produced
 * by the program being traced. Outputs only the memory reference lines in
 * between the two markers
 */

int main(int argc, char **argv) {
    
    if(argc != 3) {
         fprintf(stderr, "Usage: %s tracefile markerfile\n", argv[0]);
         exit(1);
    }

    unsigned long start_marker, end_marker, addr;
    FILE *trace = fopen(argv[1], "r");
    FILE *markers = fopen(argv[2], "r");
    fscanf(markers, "%lx %lx", &start_marker, &end_marker);
    
    char access;
    int size;
    int check = 0;
    while (fscanf(trace, " %c %lx,%d\n", &access, &addr, &size) != EOF) {
        if (addr == end_marker) {
            check = 0;
        } else if (addr == start_marker) {
            check = 1;
        } else {
            if (check) {
                printf("%c,%#lx\n", access, addr);
        }
        }
    }
    fclose(trace);
    fclose(markers);
    return 0;
}
