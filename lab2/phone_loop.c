#include <stdio.h>
#include <stdlib.h>

int phone_loop() {
    char phone[11];
    int code;
    int error = 0;
    printf("Enter phone number:");
    scanf("%s", phone);
    printf("Enter dial code:");
    while (scanf("%d", &code) != EOF) {
        if (code == -1) {
            printf("%s\n", phone);
        } else if (code >=0 && code <= 9) {
            printf("%c\n", phone[code]);
        } else {
            printf("ERROR\n");
            error = 1;
        }
        printf("Enter dial code:");
    }
    return error;
}

int main() {
   int return_code = phone_loop();
   return return_code;
}