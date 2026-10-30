// MathOracle — SecureAI's automated math oracle
// Task: buffer overflow + format string bug
// Teacher build: gcc -g -w -o mathoracle mathoracle.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct account {
    char name[16];
    int balance;
};

void fermat(void) {
    char flag[64] = {0};
    FILE *f = fopen("./flag.txt", "r");
    if (!f)
        exit(1);
    fgets(flag, sizeof(flag), f);
    printf("MARGIN NOTE RECOVERED: %s", flag);
    exit(0);
}

void feedback(char *fb) {
    ssize_t n = read(0, fb, 120);
    if (n <= 0)
        return;
    fb[n] = '\0';
    printf("Your feedback: ");
    printf(fb);
    fflush(stdout);
}

void banner(void) {
    puts("  .-----------------------------------------------.");
    puts("  |               SecureAI MathOracle             |");
    puts("  |           The Automated Math Oracle           |");
    puts("  |===============================================|");
    puts("  |                                               |");
    puts("  |       Status: ONLINE   Credit: ENABLED        |");
    puts("  |                                               |");
    puts("  '-----------------------------------------------'");
    puts("                \\___________________/");
}

void chat(void) {

    struct account user = {0};
    char fb[128], content[256];

    printf("This is your first time here, and we will give you a 10 experience credit.\n");
    user.balance = 10;
    printf("Your name: ");
    read(0, user.name, 0x20);
    printf("Welcome, %s\n", user.name);
    printf("Your balance: %d\n", user.balance);

    if (!user.balance) {
        printf("You have no credit, and you can only leave feedback.\n");
        feedback(fb);
    }

    puts("Now you can ask me any math problems, and I will answer you.");
    read(0, content, 0x160);
    puts("Thinking...");
    if (user.balance <= 0)
        puts("Sorry, you don't have enough credit to use our AI system.");
    else
        puts("I have discovered a truly marvelous proof, but this screen is too small to contain it. :)");
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin, NULL, _IONBF, 0);
    banner();
    chat();
    return 0;
}
