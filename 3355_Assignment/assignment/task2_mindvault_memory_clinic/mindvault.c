// MindVault — the long-term memory clinic
// Task: use-after-free (tcache fd) + out-of-bounds heap read
// Teacher build: gcc -g -o mindvault mindvault.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXNOTES  8
#define NOTE_SIZE 96      // every memory is exactly this big (chunk 0x70)
#define MAXDUMP   512

struct note {
    long len;             // first 8 bytes of the chunk: how much to show
    char data[];          // the memory itself
};

struct note *mem[MAXNOTES];

void banner(void) {
    puts("  .-----------------------------------------------.");
    puts("  |                    MindVault                  |");
    puts("  |         the long-term memory clinic           |");
    puts("  |      \"we free your memories, mostly\"          |");
    puts("  '-----------------------------------------------'");
}

void store(void) {
    int i = 0;
    while (i < MAXNOTES && mem[i])
        i++;
    if (i == MAXNOTES) {
        puts("Vault is full.");
        return;
    }
    struct note *n = calloc(1, sizeof(long) + NOTE_SIZE);
    if (!n)
        exit(1);
    n->len = NOTE_SIZE;
    printf("content: ");
    fflush(stdout);
    fgets(n->data, NOTE_SIZE, stdin);
    mem[i] = n;
    printf("Stored as memory %d.\n", i);
}

void show(void) {
    printf("index: ");
    fflush(stdout);
    char line[16];
    if (!fgets(line, sizeof(line), stdin))
        exit(0);
    int idx = atoi(line);
    if (idx < 0 || idx >= MAXNOTES || !mem[idx]) {
        puts("(no such memory)");
        return;
    }
    struct note *n = mem[idx];
    long len = n->len;
    if (len <= 0) {
        puts("(empty note)");
        return;
    }
    if (len > MAXDUMP)
        len = MAXDUMP;
    write(1, n->data, len);
    puts("");
}

void forget(void) {
    printf("index: ");
    fflush(stdout);
    char line[16];
    if (!fgets(line, sizeof(line), stdin))
        exit(0);
    int idx = atoi(line);
    if (idx < 0 || idx >= MAXNOTES || !mem[idx]) {
        puts("(no such memory)");
        return;
    }
    free(mem[idx]);                    // freed... but are we really letting go?
    puts("Memory deleted. It's gone forever. Trust us.");
}

void infer(void) {
    char flagbuf[48] = {0};
    char *secret = malloc(64);         // private scratch pad (chunk 0x50)
    FILE *f = fopen("./flag.txt", "r");
    if (!secret || !f)
        exit(1);
    fgets(flagbuf, sizeof(flagbuf), f);
    fclose(f);
    snprintf(secret, 64, "TOP SECRET: %s", flagbuf);
    puts("Inference complete. The result is classified.");
    // `secret` is intentionally leaked: the AI never forgets,
    // and no menu option will ever hand you this pointer.
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin, NULL, _IONBF, 0);
    banner();
    for (;;) {
        puts("[ 1:store  2:read  3:delete  4:infer  0:exit ]");
        printf("> ");
        char line[16];
        if (!fgets(line, sizeof(line), stdin))
            break;
        switch (atoi(line)) {
        case 1: store();  break;
        case 2: show();   break;
        case 3: forget(); break;
        case 4: infer();  break;
        case 0: puts("MindVault forgets everything. Except what it doesn't."); return 0;
        default: puts("?"); break;
        }
    }
    return 0;
}
