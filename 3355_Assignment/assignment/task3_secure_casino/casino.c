// Secure Casino — the house always wins
// Task: integer overflow + TOCTOU (check-then-use file race)
// Teacher build: gcc -g -o casino casino.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/random.h>

#define ENTRY_FEE 1000000
#define PROFILE   "./profile.dat"
#define MAXQTY    65535        /* cash register limit */
#define TABLE_MAX 100000       /* slots table limit  */

static int g_balance = 100;
static int g_key = 0;
static int g_badge = 0;
static unsigned long long g_nonce = 0;   /* from getrandom(), never printed */
static char g_name[32];

static const char *item_name[]  = {"Espresso", "Lucky Charm", "Office Supplies", "Vault Key"};
static const int   item_price[] = {50, 777, 430000, -1};
#define NITEMS 4

static unsigned long long fnv64(const unsigned char *data, size_t len) {
    unsigned long long h = 1469598103934665603ull;
    for (size_t i = 0; i < len; i++) {
        h ^= data[i];
        h *= 1099511628211ull;
    }
    return h;
}

/* The nonce goes AFTER the message: every FNV step is reversible
 * (h_prev = h*P^-1 ^ b), so a secret prefix could be "peeled off"
 * from a known (message, signature) pair and reused to forge.
 * With the secret at the end, forging needs a 2^64 nonce search. */
static unsigned long long sign_line(const char *line1) {
    unsigned char buf[160];
    size_t n = strlen(line1);
    if (n > sizeof(buf) - sizeof(g_nonce))
        exit(1);
    memcpy(buf, line1, n);
    memcpy(buf + n, &g_nonce, sizeof(g_nonce));
    return fnv64(buf, n + sizeof(g_nonce));
}

static void banner(void) {
    puts("  .-----------------------------------------------.");
    puts("  |                 Secure Casino                 |");
    puts("  |           \"the house always wins\"             |");
    puts("  |===============================================|");
    puts("  |       slots - shop - work - vault             |");
    puts("  '-----------------------------------------------'");
}

static void new_profile(void) {
    char line1[128];
    snprintf(line1, sizeof(line1), "K=%d B=%d N=%s", g_key, g_badge, g_name);
    FILE *f = fopen(PROFILE, "w");
    if (!f)
        exit(1);
    fprintf(f, "%s\nSIG=%016llX\n", line1, sign_line(line1));
    fclose(f);
}

static int ask_int(const char *prompt) {
    printf("%s", prompt);
    fflush(stdout);
    char line[16];
    if (!fgets(line, sizeof(line), stdin))
        exit(0);
    return atoi(line);
}

static void slots(void) {
    int bet = ask_int("bet: ");
    if (bet <= 0 || bet > g_balance || bet > TABLE_MAX) {
        puts("The house shakes its head. (0 < bet <= cash, table limit $100000)");
        return;
    }
    g_balance -= bet;
    int r = rand() % 100;
    int win = 0;
    if (r >= 95)      win = bet * 10;
    else if (r >= 60) win = bet * 2;
    if (win) {
        g_balance += win;
        printf("[ * * * ]  You win $%d! Cash: $%d\n", win, g_balance);
    } else {
        printf("[ . . . ]  You lose $%d. The house smiles.\n", bet);
    }
}

static void shop(void) {
    puts("  #  item             price");
    for (int i = 0; i < NITEMS; i++) {
        if (item_price[i] < 0)
            printf("  %d  %-16s NOT FOR SALE\n", i, item_name[i]);
        else
            printf("  %d  %-16s $%d\n", i, item_name[i], item_price[i]);
    }
    int id = ask_int("buy item: ");
    if (id < 0 || id >= NITEMS)
        return;
    if (item_price[id] < 0) {
        puts("Vault Keys are issued to authorized members only.");
        return;
    }
    int qty = ask_int("qty (1-65535): ");
    if (qty < 1 || qty > MAXQTY) {
        puts("The cash register refuses.");
        return;
    }
    int total = item_price[id] * qty;      /* the register is old and 32-bit */
    if (total > g_balance) {
        puts("Not enough cash.");
        return;
    }
    g_balance -= total;
    printf("You bought %dx %s for $%d. Cash: $%d\n",
           qty, item_name[id], total, g_balance);
}

static void work(void) {
    g_balance += 1;
    puts("You polished the slot machines for an hour. +$1.");
    if (g_balance < ENTRY_FEE)
        printf("Only $%d to go for the vault entry fee.\n", ENTRY_FEE - g_balance);
}

static void show_profile(void) {
    printf("In memory : name=%s key=%d badge=%d cash=$%d\n",
           g_name, g_key, g_badge, g_balance);
    printf("On file   (%s):\n", PROFILE);
    FILE *f = fopen(PROFILE, "r");
    if (!f) {
        puts("  (missing)");
        return;
    }
    char buf[256];
    while (fgets(buf, sizeof(buf), f))
        printf("  %s", buf);
    fclose(f);
}

static void reload(void) {
    char line1[128], line2[64];

    /* --- check: is the profile on file authentic? --- */
    FILE *f = fopen(PROFILE, "r");
    if (!f) {
        puts("No profile on file.");
        return;
    }
    if (!fgets(line1, sizeof(line1), f) || !fgets(line2, sizeof(line2), f)) {
        fclose(f);
        puts("Corrupt profile.");
        return;
    }
    fclose(f);
    line1[strcspn(line1, "\n")] = 0;
    unsigned long long sig = 0;
    if (sscanf(line2, "SIG=%llX", &sig) != 1) {
        puts("Corrupt profile.");
        return;
    }
    if (sig != sign_line(line1)) {
        puts("Invalid signature. Nice try.");
        return;
    }
    puts("Signature OK. Contacting notary server to apply the record...");
    usleep(300000);                      /* the notary server is very slow */

    /* --- use: signature already verified, no need to check twice --- */
    FILE *f2 = fopen(PROFILE, "r");
    if (!f2) {
        puts("Profile vanished?!");
        return;
    }
    if (fscanf(f2, "K=%d B=%d", &g_key, &g_badge) != 2) {
        puts("Corrupt profile.");
        fclose(f2);
        return;
    }
    fclose(f2);
    printf("Profile applied: key=%d badge=%d\n", g_key, g_badge);
}

static void vault(void) {
    puts(" .-----------------.");
    puts(" |   SECURE VAULT  |");
    puts(" '-----------------'");
    if (!g_key) {
        printf("The door ignores you. (badge on file says K=%d - not authorized)\n", g_key);
        return;
    }
    if (g_balance < ENTRY_FEE) {
        printf("Authorized, but the entry fee is $%d. You have $%d.\n",
               ENTRY_FEE, g_balance);
        return;
    }
    char flag[64] = {0};
    FILE *f = fopen("./flag.txt", "r");
    if (!f)
        exit(1);
    fgets(flag, sizeof(flag), f);
    printf("VAULT OPEN. The house's one true secret:\n%s", flag);
    exit(0);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin, NULL, _IONBF, 0);
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    if (getrandom(&g_nonce, sizeof(g_nonce), 0) != (ssize_t)sizeof(g_nonce))
        exit(1);   /* no predictable nonce: the signature must be unforgeable offline */

    banner();
    printf("Your gambler name: ");
    if (!fgets(g_name, sizeof(g_name), stdin))
        return 0;
    g_name[strcspn(g_name, "\n")] = 0;
    if (!g_name[0])
        strcpy(g_name, "anonymous");
    new_profile();
    printf("A fresh profile was written to %s. Good luck, %s.\n", PROFILE, g_name);

    for (;;) {
        puts("[ 1:slots  2:shop  3:work  4:profile  5:reload  6:vault  0:exit ]");
        printf("cash: $%d\n> ", g_balance);
        char line[16];
        if (!fgets(line, sizeof(line), stdin))
            break;
        switch (atoi(line)) {
        case 1: slots();       break;
        case 2: shop();        break;
        case 3: work();        break;
        case 4: show_profile();break;
        case 5: reload();      break;
        case 6: vault();       break;
        case 0: puts("The house thanks you for your donation."); return 0;
        default: puts("?"); break;
        }
    }
    return 0;
}
