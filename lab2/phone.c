#include <stdio.h>
#include <stdlib.h>

int phone() {
    char phone[11];
    int code;
    printf("Enter phone number:");
    scanf("%s", phone);
    printf("Enter dial code:");
    scanf("%d", &code);
    if (code == -1) {
        printf("%s", phone);
        return 0;
    } else if (code >=0 && code <= 9) {
        printf("%c", phone[code]);
        return 0;
    } else {
        printf("ERROR");
        return 1;
    }
}

int main() {
    phone();
    return 0;
}