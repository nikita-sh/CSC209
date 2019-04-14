#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include "helper.h"

/* 
 * Performs error checking for calls to fopen. If an error occurs, 
 * prints errno and exits, otherwise, returns new file pointer.
*/
FILE *fopen_or_exit(char *file, char *mode) {
    FILE *fp = fopen(file, mode);
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    return fp;
}

/* 
 * Performs error checking for calls to fclose. If an error occurs, 
 * prints errno and exits.
*/
void fclose_or_exit(FILE *fp) {
    if (fclose(fp) == EOF) {
        perror("fclose");
        exit(1);
    }
}

/*
 * Performs error checking for calls to fork(). If an error occurs,
 * prints errno and exits, otherwise returns the return value of 
 * fork.
 */
int fork_or_exit() {
    int fork_ret = fork();
    if (fork_ret < 0) {
        perror("fork");
        exit(1);
    }
    return fork_ret;
}

/*
 * Performs error checking for calls to pipe(). If an error occurs,
 * prints errno and exits, otherwise returns the new file descriptors.
 */
int pipe_or_exit(int *fd) {
    if (pipe(fd) < 0) {
        perror("pipe");
        exit(1);
    }
    return 0;
}

/*
 * Performs error checking for calls to close(). If an error occurs,
 * prints errno and exits, otherwise returns the return value of close().
 */
int close_or_exit(int *fd, int index) {
    if (close(fd[index]) == -1) {
        perror("close");
        exit(1);
    }
    return 0;
}

/*
 * Performs error checking for calls to wait(). If an error occurs,
 * prints errno and exits, otherwise returns the id of the terminated process.
 */
int wait_or_exit(int *status) {
    int w = wait(status);
    if (w == -1) {
        perror("wait");
        exit(1);
    }
    return w;
}

/*
 * Performs error checking for calls to malloc(). If an error occurs,
 * prints errno and exits, otherwise returns a pointer to the newly allocated memory.
 */
void *malloc_or_exit(int size) {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        perror("malloc");
        exit(1);
    }
    return ptr;
}

int fseek_or_exit(FILE *stream, long offset, int whence) {
    if (fseek(stream, offset, whence) == -1) {
        perror("fseek");
        exit(1);
    }
    return 0;
}

/*
 * Function that, given pointers to the input and output file names, as well as the number
 * of processes, sets the pointers to point to the correct values. It also takes as arguments
 * pointers to the argument count and argument vector.
 */
void get_args(int *argc, char ***argv, char **input, char **output, int *num_proc) {
    char opt;
    char *end_ptr;
    while ((opt = getopt(*argc, *argv, "n:f:o:")) != -1) {
        switch (opt) {
            case 'n':
                *num_proc = strtol(optarg, &end_ptr, 10);
                if (optarg == end_ptr || *end_ptr != '\0' || *num_proc == LONG_MAX || *num_proc == LONG_MIN) {
                    fprintf(stderr, "strtol: Invalid arguments\n");
                    exit(1);
                }
                break;
            case 'f':
                *input = optarg;
                break;
            case 'o':
                *output = optarg;
                break;
            default:
                fprintf(stderr, "Usage: psort -n <number of processes> -f <inputfile> -o <outputfile>\n");
                exit(1);
        }
    }
}

/*
 * Returns the index of the smallest element in an array of
 * struct rec's based on frequency.
 */
int smallest_index(struct rec *rec_list, int size) {
    int smallest = 0;
    for (int i = 1; i < size; i++) {
        if ((rec_list[i].freq > 0 && rec_list[i].freq < rec_list[smallest].freq) || rec_list[smallest].freq < 0) {
            smallest = i;
        }
    }
    return smallest;
}

/*
 * Returns a pointer to dynamically allocated memory containing the records
 * starting at start and ending at end in the the file input_file.
 */
struct rec *read_rec_block(int start, int end, FILE *input_file) {
    int size = (end - start + 1) / sizeof(struct rec);
    // malloc to avoid running out of stack memory on large intervals
    struct rec *return_list = malloc_or_exit(sizeof(struct rec)*size);
    fseek_or_exit(input_file, start, SEEK_SET);
    if (fread(return_list, sizeof(struct rec), size, input_file) != size) {
        fprintf(stderr, "fread: Failed to properly read item.\n");
        exit(1);
    }
    return return_list;
}

/*
 * Function that gets the interval for the chlid numbered by iteration.
 * Puts the interval in the provided pointer to int ret. Puts n_rec/num_proc + 1
 * records in n_rec % num_proc children and n_rec/num_proc in the rest.
 */ 
void get_interval(int iteration, int num_proc, int n_rec, int *ret) {
    int block_size;
    if (iteration > (n_rec % num_proc)) {
        block_size = n_rec / num_proc;
        get_interval(iteration - 1, num_proc, n_rec, ret);
        ret[0] = ret[1] + 1;
        ret[1] = ret[1] + sizeof(struct rec) * block_size;
    } else {
        block_size = n_rec / num_proc + 1;
        if (iteration == 1) {
            ret[0] = 0;
            ret[1] = sizeof(struct rec)*block_size - 1;
        } else {
            ret[0] = sizeof(struct rec)*block_size*(iteration - 1);
            ret[1] = sizeof(struct rec)*block_size*(iteration) - 1;
        }
    }
}
 /*
  * Function that performs all reading, sorting, and writing for a child process. Retrieves 
  * interval based on child, num_proc and num_rec from input_fp, and writes it to the appropriate
  * pipe in fd based on child.
  */
void sort_and_write(int *fd, int child, int num_proc, int num_rec, FILE *input_fp) {
    int interval[2];
    int size;

    // gets interval of this child
    get_interval(child, num_proc, num_rec, interval);
    size = (interval[1] - interval[0] + 1) / sizeof(struct rec);
    // returns a dynamically allocated array containing the records for the current interval
    struct rec *rec_list = read_rec_block(interval[0], interval[1], input_fp);
    
    // sort records array with provided comparison function
    qsort(rec_list, size, sizeof(struct rec), compare_freq);
    
    // write array to pipe
    // on large inputs, this will block, but since we wait for children after merges in the parent, it works.
    for (int j = 0; j < size; j++) {
        if (write(fd[1], &rec_list[j], sizeof(struct rec)) == -1) {
            perror("write");
            exit(1);
        }
    }
    // free alloc'd array
    free(rec_list);
    // done writing, close and exit
    close_or_exit(fd, 1);
}

/*
 * Function to check all indices of a given records array to see if they
 * are all sentinel records. Returns 1 if there are still valid records
 * and 0 otherwise.
 */
int check_sentinel(struct rec *rec_array, int size) {
    int check = 0;
    for (int i = 0; i < size; i++) {
        if (rec_array[i].freq != -1) {
            check = 1;
        }
    }
    return check;
}

/*
 * Helper function that reads from all pipes and writes record with smallest frequency to output file.
 * If a pipe is found to be empty, either through abnormal termination or due to having written all of 
 * the necessary elements, it's index in the smallest array is replaced with a sentinel record. The sentinel
 * record has an impossible frequency (-1), and is never chose as the smallest index.
 */
void merge(FILE *output_fp, int fd[][2], int num_proc) {
    // malloc array of smallest values. avoid ENOMEM errors for a large number of processes
    struct rec *smallest = malloc_or_exit(num_proc * sizeof(struct rec));

    // initialize sentinel struct
    struct rec sentinel = {-1, "sentinel"};
    int smallest_i, flag, num_bytes;

    // read one element from every pipe
    // if some error occurred in child and didnt write, immediately place sentinel
    for (int i = 0; i < num_proc; i++) {
        num_bytes = read(fd[i][0], &smallest[i], sizeof(struct rec));
        if (num_bytes == -1) {
            perror("read");
            exit(1);
        } else if (num_bytes == 0) {
            smallest[i] = sentinel;
        }
    }
    
    // loop and choose record with smallest frequency from array smallest
    // loops until all values are sentinel records
    flag = 1;
    while (flag) {
        // get smallest index
        smallest_i = smallest_index(smallest, num_proc);
        if (fwrite(&smallest[smallest_i], sizeof(struct rec), 1, output_fp) != 1) {
            fprintf(stderr, "fwrite: Failed to properly write item.");
            exit(1);
        }
        int read_status = read(fd[smallest_i][0], &smallest[smallest_i], sizeof(struct rec));
        if (read_status == 0) {
            smallest[smallest_i] = sentinel;
        } else if (read_status == -1) {
            perror("read");
            exit(1);
        }
        // check if all values are sentinel records
        flag = check_sentinel(smallest, num_proc);
    }
    // free alloc'd memory
    free(smallest);
}



int main(int argc, char **argv) {
    if (argc != 7) {
        fprintf(stderr, "Usage: psort -n <number of processes> -f <inputfile> -o <outputfile>\n");
        exit(1);
    }

    int return_code = 0;
    int num_proc, fsize, n_rec, fork_ret, i, status;
    char *input, *output;
    FILE *input_fp, *output_fp;

    // Get arguments using helper
    get_args(&argc, &argv, &input, &output, &num_proc);

    fsize = get_file_size(input);

    // If file is empty, no work to be done. Open and close output file to create it.
    if (fsize == 0) {
        output_fp = fopen_or_exit(output, "wb");
        fclose_or_exit(output_fp);
        return 0;
    }

    n_rec = fsize / sizeof(struct rec);

    // Handle problem values for num_proc
    if (num_proc > n_rec) {
        num_proc = n_rec;
    } else if (num_proc <= 0) {
        num_proc = 1;
    }

    int fd[num_proc][2];
    for (i = 0; i < num_proc; i++) {
        // create pipe and fork child process
        pipe_or_exit(fd[i]);
        fork_ret = fork_or_exit();
        // child
        if (fork_ret == 0) {
            input_fp = fopen_or_exit(input, "rb");
            // close read end
            close_or_exit(fd[i], 0);
            // close read ends of previously forked children
            for (int j = 0; j < i; j++) {
                close_or_exit(fd[j], 0);
            }
            // Call function to sort and write to pipe
            sort_and_write(fd[i], i+1, num_proc, n_rec, input_fp);
            // Done reading input, close FILE *
            fclose_or_exit(input_fp);
            exit(0);
        } else {
            // close write end of pipe
            close_or_exit(fd[i], 1);
        }
    }

    // Call merge function to handle reading from all children and writing in sorted order
    output_fp = fopen_or_exit(output, "wb");
    merge(output_fp, fd, num_proc);

    // Done writing. Close pipes and file pointer.
    fclose_or_exit(output_fp);
    for (i = 0; i < num_proc; i++) {
        close_or_exit(fd[i], 0);
    }

    // Wait for children to check if they terminated abnormally
    // Can't do this before merging as write may block on large inputs
    for (i = 0; i < num_proc; i++) {
        wait_or_exit(&status);
        if (!WIFEXITED(status)) {
            return_code = 1;
            fprintf(stderr, "Child terminated abnormally\n");
        } else if (WEXITSTATUS(status) != 0) {
            return_code = 1;
        }
    }
    return return_code;
}
