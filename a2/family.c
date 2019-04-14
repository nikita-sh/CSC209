#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "family.h"

/* Number of word pointers allocated for a new family.
   This is also the number of word pointers added to a family
   using realloc, when the family is full.
*/
static int family_increment = 0;


/* Set family_increment to size, and initialize random number generator.
   The random number generator is used to select a random word from a family.
   This function should be called exactly once, on startup.
*/
void init_family(int size) {
    family_increment = size;
    srand(time(0));
}


/* Given a pointer to the head of a linked list of Family nodes,
   print each family's signature and words.

   Do not modify this function. It will be used for marking.
*/
void print_families(Family* fam_list) {
    int i;
    Family *fam = fam_list;
    
    while (fam) {
        printf("***Family signature: %s Num words: %d\n",
               fam->signature, fam->num_words);
        for(i = 0; i < fam->num_words; i++) {
            printf("     %s\n", fam->word_ptrs[i]);
        }
        printf("\n");
        fam = fam->next;
    }
}


/* Return a pointer to a new family whose signature is 
   a copy of str. Initialize word_ptrs to point to 
   family_increment+1 pointers, numwords to 0, 
   maxwords to family_increment, and next to NULL.
*/
Family *new_family(char *str) {
    Family *ret_fam = malloc(sizeof(Family));
    ret_fam->signature = malloc(sizeof(char) * strlen(str) + 1);
    ret_fam->word_ptrs = malloc(sizeof(char *) * (family_increment + 1));

    if (!ret_fam->signature || !ret_fam || !ret_fam->word_ptrs) {
        perror("malloc");
        exit(1);
    }

    // Safe because we allocated exactly enough memory for signature
    strcpy(ret_fam->signature, str);
    ret_fam->word_ptrs[0] = NULL;
    ret_fam->num_words = 0;
    ret_fam->max_words = family_increment;
    ret_fam->next = NULL;
    return ret_fam;
}


/* Add word to the next free slot fam->word_ptrs.
   If fam->word_ptrs is full, first use realloc to allocate family_increment
   more pointers and then add the new pointer.
*/
void add_word_to_family(Family *fam, char *word) {
    if (fam->num_words >= fam->max_words) {
        fam->word_ptrs = realloc(fam->word_ptrs, (sizeof(char *) * fam->num_words) + (sizeof(char *) * (family_increment + 1)));

        if (!fam->word_ptrs) {
            perror("realloc");
            exit(1);
        }

        fam->max_words = fam->max_words + family_increment;
    }
    fam->word_ptrs[fam->num_words] = word;
    fam->word_ptrs[fam->num_words+1] = NULL;
    fam->num_words++;
}


/* Return a pointer to the family whose signature is sig;
   if there is no such family, return NULL.
   fam_list is a pointer to the head of a list of Family nodes.
*/
Family *find_family(Family *fam_list, char *sig) {
    Family *curr_fam = fam_list;
    while (curr_fam) {
        // All strings null terminated, so strcmp is safe
        if (strcmp(curr_fam->signature, sig) == 0) {
            return curr_fam;
        }
        curr_fam = curr_fam->next;
    }
    return NULL;
}


/* Return a pointer to the family in the list with the most words;
   if the list is empty, return NULL. If multiple families have the most words,
   return a pointer to any of them.
   fam_list is a pointer to the head of a list of Family nodes.
*/
Family *find_biggest_family(Family *fam_list) {
    if (!fam_list) {
        return NULL;
    } else {
        Family *curr = fam_list;
        Family *largest = curr;
        while (curr) {
            if (curr->num_words > largest->num_words) {
                largest = curr;
            }
            curr = curr->next;
        }
        return largest;
    }
}


/* Deallocate all memory rooted in the List pointed to by fam_list. */
void deallocate_families(Family *fam_list) {
    Family *curr = fam_list;
    Family *curr_free;
    while (curr) {
        free(curr->signature);
        free(curr->word_ptrs);
        curr_free = curr;
        curr = curr->next;
        free(curr_free);
    }
}


/* Generate and return a linked list of all families using words pointed to
   by word_list, using letter to partition the words.

   Implementation tips: To decide the family in which each word belongs, you
   will need to generate the signature of each word. Create only the families
   that have at least one word from the current word_list.
*/
Family *generate_families(char **word_list, char letter) {
    int i = 0;
    char **signatures;
    //  All strings null terminated, so strlen is safe
    int word_length = strlen(word_list[0]);
    int length = 0;
    while (word_list[i]) {
        length++;
        i++;
    }
    // Generate signatures
    signatures = malloc(sizeof(char *) * length);

    if (!signatures) {
        perror("malloc");
        exit(1);
    }

    for (i = 0; i < length; i++) {
        signatures[i] = malloc(word_length + 1);

        if (!signatures[i]) {
            perror("malloc");
            exit(1);
        }

        for (int j = 0; j < word_length; j++) {
            if (word_list[i][j] != letter) {
                signatures[i][j] = '-';
            } else {
                signatures[i][j] = word_list[i][j];
            }
        }
        // Making sure signature strings are null terminated
        signatures[i][word_length] = '\0';
    }
    // Generate families for signatures
    Family *head = new_family(signatures[0]);
    Family *curr = head;
    for (i = 1; i < length; i++) {
        if (!find_family(head, signatures[i])) {
            curr->next = new_family(signatures[i]);
            curr = curr->next;
        }
    }
    // Add words to families
    for (i = 0; i < length; i++) {
        curr = find_family(head, signatures[i]);
        add_word_to_family(curr, word_list[i]);
    }
    // Deallocate signatures
    for (i = 0; i < length; i++) {
        free(signatures[i]);
    }
    free(signatures);
    return head;
}


/* Return the signature of the family pointed to by fam. */
char *get_family_signature(Family *fam) {
    return fam->signature;
}


/* Return a pointer to word pointers, each of which
   points to a word in fam. These pointers should not be the same
   as those used by fam->word_ptrs (i.e. they should be independently malloc'd),
   because fam->word_ptrs can move during a realloc.
   As with fam->word_ptrs, the final pointer should be NULL.
*/
char **get_new_word_list(Family *fam) {
    char **new_words = malloc((sizeof(char *) * fam->num_words) + sizeof(void *));
    
    if (!new_words) {
        perror("malloc");
        exit(1);
    }
    
    for (int i = 0; i < fam->num_words; i++) {
        new_words[i] = fam->word_ptrs[i];
    }
    new_words[fam->num_words] = NULL;
    return new_words;
}


/* Return a pointer to a random word from fam. 
   Use rand (man 3 rand) to generate random integers.
*/
char *get_random_word_from_family(Family *fam) {
    int index = rand() % fam->num_words;
    return fam->word_ptrs[index];
}