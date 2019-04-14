#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAXLINE 256
#define MAX_PASSWORD 10

#define SUCCESS "Password verified\n"
#define INVALID "Invalid password\n"
#define NO_USER "No such user\n"

int main(void) {
    char user_id[MAXLINE];
    char password[MAXLINE];

    if(fgets(user_id, MAXLINE, stdin) == NULL) {
        perror("fgets");
        exit(1);
    }
    if(fgets(password, MAXLINE, stdin) == NULL) {
        perror("fgets");
        exit(1);
    }

	// open pipe
    int fd[2];
    if (pipe(fd) < 0) {
        perror("pipe");
    	exit(1);
    }

	// create child process
    int r = fork();
    if (r < 0) {
    	perror("fork");
    	exit(1);
    }

	// parent
    if (r > 0) {
		// parent wont read from pipe, close it
		if (close(fd[0]) == -1) {
			perror("close");
			exit(1);
		}
		// write user and pass to pipe
		if (write(fd[1], user_id, MAX_PASSWORD) == -1) {
			perror("write");
			exit(1);
		}
		if (write(fd[1], password, MAX_PASSWORD) == -1) {
			perror("write");
			exit(1);
		}
		if (close(fd[1]) == -1) {
			perror("close");
			exit(1);
		}
	// child
	} else {
		// redirect stdin of child process to come from pipe
		if (dup2(fd[0], STDIN_FILENO) == -1) {
			perror("dup2");
			exit(1);
		}
		// file descriptors no longer needed, close them
		if (close(fd[0]) == -1) {
			perror("close");
			exit(1);
		}
		if (close(fd[1]) == -1) {
			perror("close");
			exit(1);
		}
		// execute validate
		execl("./validate", "validate", NULL);
		perror("execl");
		exit(1);
	}

	int status;
	// want child to verify user:pass before printing to stdout
	wait(&status);
	if (WIFEXITED(status)) {
		int exit_code = WEXITSTATUS(status);
		switch(exit_code) {
			case 0:
				printf(SUCCESS);
				break;
			case 2:
				printf(INVALID);
				break;
			case 3:
				printf(NO_USER);
				break;
			default:
				break;
		}
	}
    return 0;
}
