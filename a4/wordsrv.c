#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#include "socket.h"
#include "gameplay.h"


#ifndef PORT
    #define PORT 56480
#endif
#define MAX_QUEUE 5


void add_player(struct client **top, int fd, struct in_addr addr);
void remove_player(struct client **top, int fd);
void advance_turn(struct game_state *game);
void write_msg(char *msg, struct client *p, struct game_state *game);
void announce_turn(struct game_state *game);

/* Send the message in outbuf to all clients */
void broadcast(struct game_state *game, char *outbuf) {
    for (struct client *p = game->head; p != NULL; p = p->next) {
        write_msg(outbuf, p, game); 
    }
}

/*
 * Writes msg to client p. Handles client disconnect. Assumes that
 * the client is in the list of active players.
*/
void write_msg(char *msg, struct client *p, struct game_state *game) {
    int num_written = write(p->fd, msg, strlen(msg));
    if (num_written == -1 || num_written != strlen(msg)) {
        char out[MAX_MSG];

        // Notify server of disconnect.
        printf("Disconnected from %s\n", inet_ntoa(p->ipaddr));

        // Advance turn if we are removing player whose turn it is
        if (game->has_next_turn == p) {
            advance_turn(game);
        }

        // Remove the player
        remove_player(&game->head, p->fd);

        // Make sure game is playable for any clients in new_players
        if (game->head == NULL) {
            game->has_next_turn = NULL;
        }

        // Broadcast goodbye message to any active clients
        sprintf(out, "Goodbye %s.\r\n", p->name);
        broadcast(game, out);
        announce_turn(game);
    }
}

/* Move the has_next_turn pointer to the next active client */
void advance_turn(struct game_state *game) {
    if (game->has_next_turn->next) {
        game->has_next_turn = game->has_next_turn->next;
    } else {
        game->has_next_turn = game->head;
    } 
}

/*
 * Announces which players turn it is to all active clients 
 * except that player, to which it prompts for a guess.
*/
void announce_turn(struct game_state *game) {
    struct client *p;
    char msg[MAX_MSG];

    for (p = game->head; p; p = p->next) {
        if (p != game->has_next_turn) {
            sprintf(msg, "It's %s's turn.\r\n", game->has_next_turn->name);
        } else {
            sprintf(msg, "Your guess?\r\n");
            printf("Its %s's turn.\n", game->has_next_turn->name);
        }
        write_msg(msg, p, game);
    }
}

/*
 * Looks for a network newline in buf. Returns the index of the \n in buf if 
 * such a newline exists, and -1 otherwise.
*/
int find_network_newline(char *buf, int size) {
    for (int i = 0; i < size; i++) {
        if (i > 0 && buf[i] == '\n' && buf[i-1] == '\r') {
            return i;
        }
    }
    return -1;
}

/*
 * Checks if name is valid. Returns -1 if the name is taken, too long, or
 * an empty string. Returns 0 otherwise.
*/
int check_name(char *name, struct game_state *game) {
    for (struct client *curr = game->head; curr != NULL; curr = curr->next) {
        if (strcmp(curr->name, name) == 0 || strlen(name) > MAX_NAME || strlen(name) == 0) {
            return -1;
        }
    }
    return 0;
}

/*
 * Checks if a guess is valid. Returns 1 if the guess is not a single character between a and z,
 * returns 2 if the guess has already been made, and returns 0 otherwise.
*/
int check_guess(char *guess, struct game_state *game) {
    if (strlen(guess) > 1 || guess[0] < 'a' || guess[0] > 'z' || strlen(guess) == 0) {
        return 1;
    } else if (game->letters_guessed[guess[0] - 'a']) {
        return 2;
    }
    return 0;
}

/*
 * Updates game to reflect a guess being made. If the guess is incorrect,
 * writes to the player notifying them of this. Returns 1 if the guess
 * was correct and 0 otherwise.
*/
int make_guess(struct game_state *game, char guess) {
    char msg[MAX_MSG];
    int correct = 0;

    // Loop through word and update guess accordingly
    // Sets correct to be 1 if a matching letter is found
    for (int i = 0; i < strlen(game->word); i++) {
        if (game->word[i] == guess) {
            game->guess[i] = guess;
            correct = 1;
        }
    }
    
    // Guess is incorrect, notify client, print to server, update number of guesses
    if (!correct) {
        sprintf(msg, "%c is not in the word.\r\n", guess);
        write_msg(msg, game->has_next_turn, game);
        printf("Letter %c is not in the word\n", guess);
        game->guesses_left -= 1;
    }

    // Update letters guessed
    game->letters_guessed[guess - 'a'] = 1;
    return correct;
}

/*
 * Checks game_state game to see if the game is over. Returns 
 * 2 if the number of guesses is 0, returns 1 if the game has
 * been won, and returns 0 if the game is still active.
*/
int check_game_over(struct game_state *game) {
    if (game->guesses_left == 0) {
        return 2;
    }

    for (int i = 0; i < strlen(game->guess); i++) {
        if (game->guess[i] == '-') {
            return 0;
        }
    }

    return 1;
}

/* The set of socket descriptors for select to monitor.
 * This is a global variable because we need to remove socket descriptors
 * from allset when a write to a socket fails.
 */
fd_set allset;


/* 
 * Add a client to the head of the linked list
 */
void add_player(struct client **top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));

    if (!p) {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));

    p->fd = fd;
    p->ipaddr = addr;
    p->name[0] = '\0';
    p->in_ptr = p->inbuf;
    p->inbuf[0] = '\0';
    p->next = *top;
    *top = p;
}

/* Removes client from the linked list and closes its socket.
 * Also removes socket descriptor from allset 
 */
void remove_player(struct client **top, int fd) {
    struct client **p;

    for (p = top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        FD_CLR((*p)->fd, &allset);
        close((*p)->fd);
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
}

/* 
 * Removes and returns client from the linked list without closing the socket.
 * Used as an intermediate step when changing the linked lists.
 */
void temp_remove_player(struct client **top, int fd) {
    struct client **p;

    for (p = top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        *p = t;
    } else {
        fprintf(stderr, "Trying to change the list of fd %d, but I don't know about it\n",
                 fd);
    }
}


int main(int argc, char **argv) {
    // Handler for SIGPIPE
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGPIPE, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    int clientfd, maxfd, nready;
    struct client *p;
    struct sockaddr_in q;
    fd_set rset;
    
    if(argc != 2){
        fprintf(stderr,"Usage: %s <dictionary filename>\n", argv[0]);
        exit(1);
    }
    
    // Create and initialize the game state
    struct game_state game;

    srandom((unsigned int)time(NULL));
    // Set up the file pointer outside of init_game because we want to 
    // just rewind the file when we need to pick a new word
    game.dict.fp = NULL;
    game.dict.size = get_file_length(argv[1]);

    init_game(&game, argv[1]);
    
    // head and has_next_turn also don't change when a subsequent game is
    // started so we initialize them here.
    game.head = NULL;
    game.has_next_turn = NULL;
    
    /* A list of client who have not yet entered their name.  This list is
     * kept separate from the list of active players in the game, because
     * until the new playrs have entered a name, they should not have a turn
     * or receive broadcast messages.  In other words, they can't play until
     * they have a name.
     */
    struct client *new_players = NULL;
    
    struct sockaddr_in *server = init_server_addr(PORT);
    int listenfd = set_up_server_socket(server, MAX_QUEUE);
    
    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (nready == -1) {
            perror("select");
            continue;
        }

        if (FD_ISSET(listenfd, &rset)){
            printf("A new client is connecting\n");
            clientfd = accept_connection(listenfd);

            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            printf("Connection from %s\n", inet_ntoa(q.sin_addr));
            add_player(&new_players, clientfd, q.sin_addr);
            char *greeting = WELCOME_MSG;
            if(write(clientfd, greeting, strlen(greeting)) == -1) {
                fprintf(stderr, "Write to client %s failed\n", inet_ntoa(q.sin_addr));
                remove_player(&new_players, clientfd);
            };
        }
        
        /* Check which other socket descriptors have something ready to read.
         * The reason we iterate over the rset descriptors at the top level and
         * search through the two lists of clients each time is that it is
         * possible that a client will be removed in the middle of one of the
         * operations. This is also why we call break after handling the input.
         * If a client has been removed the loop variables may not longer be 
         * valid.
         */
        int cur_fd;
        for(cur_fd = 0; cur_fd <= maxfd; cur_fd++) {
            if(FD_ISSET(cur_fd, &rset)) {
                // Check if this socket descriptor is an active player
                for(p = game.head; p != NULL; p = p->next) {
                    if(cur_fd == p->fd) {
                        char msg[MAX_MSG];
                        int num_read = read(cur_fd, p->in_ptr, MAX_BUF - 1);
                        
                        // Check for client disconnect
                        if (num_read <= 0) {
                            if (num_read == 0) {
                                printf("[%d] Read %d bytes\n", p->fd, num_read);
                            }

                            printf("Disconnected from %s\n", inet_ntoa(p->ipaddr));

                            // If we are removing the client whose turn it is, we need to advance the turn first
                            if (game.has_next_turn == p) {
                                advance_turn(&game);
                            }

                            remove_player(&game.head, cur_fd);

                            // Make sure game is playable for any clients in new_players
                            if (game.head == NULL) {
                                game.has_next_turn = NULL;
                            }
                            
                            // Notify other clients of disconnect
                            sprintf(msg, "Goodbye %s.\r\n", p->name);
                            broadcast(&game, msg);
                            announce_turn(&game);
                        } else {
                            printf("[%d] Read %d bytes\n", p->fd, num_read);

                            // Look for newline
                            p->in_ptr[num_read] = '\0';
                            int newline = find_network_newline(p->inbuf, strlen(p->inbuf));

                            // No newline, update in_ptr for future reads
                            if (newline == -1) {
                                p->in_ptr += num_read;
                            } else {
                                // Null terminate before newline
                                p->inbuf[newline - 1] = '\0';
                                p->in_ptr = p->inbuf;
                                
                                printf("[%d] Found newline %s\n", p->fd, p->inbuf);
                                int reset = 0;

                                // Check for players making guesses out of turn
                                if (game.has_next_turn != p) {
                                    strcpy(msg, "It's not your turn.\r\n");
                                    write_msg(msg, p, &game);
                                    printf("%s made a guess out of turn.\n", p->name);
                                } else {
                                    // Check the guess, switch on the output
                                    int valid_guess = check_guess(p->inbuf, &game);
                                    int correct;

                                    switch(valid_guess) {
                                        // Guess is valid
                                        case 0:
                                            // Make guess, evaluate for game over
                                            // If game is over, set reset for later conditional
                                            correct = make_guess(&game, p->inbuf[0]);
                                            int game_over = check_game_over(&game);

                                            // Game ends without winner.
                                            if (game_over == 2) {
                                                sprintf(msg, "No more guesses. The word was %s.\r\n", game.word);
                                                broadcast(&game, msg);
                                                reset = 1;
                                            // Game ends with winner.
                                            } else if (game_over == 1) {
                                                sprintf(msg, "The word was %s.\r\n", game.word);
                                                broadcast(&game, msg);

                                                // Different print statements for different clients
                                                sprintf(msg, "Game over! You win!\r\n");
                                                write_msg(msg, game.has_next_turn, &game);

                                                sprintf(msg, "Game over! %s won!\r\n", game.has_next_turn->name);
                                                printf("Game over! %s won\n", p->name);
                                                for (struct client *curr = game.head; curr; curr = curr->next) {
                                                    if (curr != game.has_next_turn) {
                                                        write_msg(msg, curr, &game);
                                                    }
                                                }

                                                reset = 1;
                                            // Game is still active.
                                            } else {
                                                // Broadcast the guess that was made, then broadcast updated status
                                                sprintf(msg, "%s guesses %c.\r\n", p->name, p->inbuf[0]);
                                                broadcast(&game, msg);
                                                status_message(msg, &game);
                                                broadcast(&game, msg);

                                                // Only advance the turn if the guess was incorrect
                                                if (!correct) {
                                                    advance_turn(&game);
                                                } 
                                            }
                                            break;
                                        // Guess is an invalid character.
                                        case 1:
                                            sprintf(msg, "Guesses must be a single character between a and z.\r\n");
                                            write_msg(msg, p, &game);
                                            printf("%s made an invalid guess.\n", p->name);
                                            break;
                                        // Guess has already been made.
                                        case 2:
                                            sprintf(msg, "That letter has already been guessed!\r\n");
                                            write_msg(msg, p, &game);
                                            printf("%s made an invalid guess.\n", p->name);
                                            break;
                                    }
                                    // Game is over
                                    if (reset) {
                                        // Print server message
                                        printf("New game\n");

                                        // Broadcast new game messages to clients, advance turn for new game
                                        broadcast(&game, " \r\n");
                                        broadcast(&game, "Let's start a new game.\r\n");
                                        advance_turn(&game);

                                        // Initialize new game
                                        init_game(&game, argv[1]);
                                        status_message(msg, &game);
                                        broadcast(&game, msg);
                                    }
                                    // Announce turn, prompt for guess
                                    announce_turn(&game);
                                }
                            }
                        }
                        break;
                    }
                }
        
                // Check if any new players are entering their names
                for(p = new_players; p != NULL; p = p->next) {
                    if(cur_fd == p->fd) {
                        int num_read = read(cur_fd, p->in_ptr, MAX_BUF - 1);
                        
                        // Check for client disconnect.
                        if (num_read <= 0) {
                            printf("[%d] Read %d bytes\n", p->fd, num_read);
                            printf("Disconnected from %s\n", inet_ntoa(p->ipaddr));
                            remove_player(&new_players, cur_fd);
                        } else {
                            printf("[%d] Read %d bytes\n", p->fd, num_read);

                            // Look for newline
                            p->in_ptr[num_read] = '\0';
                            int newline = find_network_newline(p->inbuf, strlen(p->inbuf));

                            // If no newline is found, update in_ptr for later reads
                            if (newline == -1) {
                                p->in_ptr += num_read;
                            } else {
                                p->inbuf[newline - 1] = '\0';
                                p->in_ptr = p->inbuf;

                                printf("[%d] Found newline %s\n", p->fd, p->inbuf);
                                
                                // Check if name is valid
                                int valid_name = check_name(p->inbuf, &game);

                                if (valid_name == 0) {
                                    // If name is valid, update name field and move them to active players
                                    temp_remove_player(&new_players, p->fd);
                                    p->next = game.head;
                                    game.head = p;
                                    strcpy(p->name, p->inbuf);

                                    // Handle turn order for first connected client
                                    if (game.has_next_turn == NULL) {
                                        game.has_next_turn = p;
                                    } 

                                    // Notify all active clients of new connection, print to server.
                                    char msg[MAX_MSG];
                                    sprintf(msg, "%s has just joined.\r\n", p->name);
                                    broadcast(&game, msg);
                                    printf("%s", msg);

                                    // Write status of game to new player.
                                    status_message(msg, &game);
                                    write_msg(msg, p, &game);

                                    // Announce the turn again
                                    announce_turn(&game);
                                } else {
                                    // Notify client the name is invalid
                                    char *msg = "Name is taken or too long.\r\n";
                                    int num_written = write(cur_fd, msg, strlen(msg));

                                    // Handle disconnect
                                    if (num_written == -1 || num_written != strlen(msg)) {
                                        printf("Disconnected from %s\n", inet_ntoa(p->ipaddr));
                                        remove_player(&new_players, cur_fd);
                                    }

                                    // Write server message and prompt for name again
                                    printf("[%d] Invalid name\n", cur_fd);
                                    sprintf(msg, "Your name?\r\n");
                                    num_written = write(cur_fd, msg, strlen(msg));

                                    // Handle disconnect
                                    if (num_written == -1 || num_written != strlen(msg)) {
                                        printf("Disconnected from %s\n", inet_ntoa(p->ipaddr));
                                        remove_player(&new_players, cur_fd);
                                    }
                                }   
                            }
                        }
                        break;
                    } 
                }
            }
        }
    }
    return 0;
}


