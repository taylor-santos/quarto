#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <bits/types/clock_t.h>
#include <time.h>
#include <stdlib.h>

#define u8  uint8_t
#define u16 uint16_t
#define u64 uint64_t

#define min(x, y) ((x) < (y)) ? (x) : (y)
#define max(x, y) ((x) < (y)) ? (y) : (x)
#define X(loc) ((loc) % 4)
#define Y(loc) ((loc) / 4)
#define boardAt(x, y) ((board >> (4u * (4u * (y) + (x)))) & 0xFu)
#define set(what, loc, data) ((what) |= (u64)(data) << 4u * (loc))
#define unset(what, loc) ((what) &= ~((u64)0xFu << 4u * (loc)))
#define isFilled(loc) (filled & (1u << (loc)))
#define isTaken(piece) (!(taken & (1u << (piece))))
#define MAX_SCORE (1000000000)

u8 rows[4] = {0};
u8 cols[4] = {0};
u8 diag[2] = {0};
u16 filled = 0;
u16 taken = ~((u16)0);
u64 board = 0;

char*
pieces[4][2] = {{ "dark",   "light" },
                { "tall",   "short" },
                { "box",    "circle" },
                { "hollow", "full" }};

static void
print_binary(u64 val, char str[], size_t size) {
    for (size_t i = 0; i < size; i++) {
        str[size - 1 - i] = (char)('0' + ((val & 1u << i) != 0));
    }
}

static void
print_piece(u8 val, char str[], u8 wins) {
    for (size_t i = 0; i < 4; i++) {
        char c = pieces[i][(val & 1u << i) != 0][0];
        if (wins & 1u << i) {
            str[i] = (char)toupper(c);
        } else {
            str[i] = c;
        }
    }
}

static void
shuffle(void *data, size_t num, size_t size, int (*rand)(void)) {
    uint8_t *data_bytes = data;
    for (int i = (int)num - 1; i >= 0; i--) {
        int j = rand() % (i + 1);
        for (size_t k = 0; k < size; k++) {
            uint8_t tmp = data_bytes[i*size + k];
            data_bytes[i*size + k] = data_bytes[j*size + k];
            data_bytes[j*size + k] = tmp;
        }
    }

}

static void
set_piece(u8 loc, u8 piece) {
    u8 x = X(loc);
    u8 y = Y(loc);
    cols[x]++;
    rows[y]++;
    if (x == y) diag[0]++;
    if (x == 3 - y) diag[1]++;
    filled |= 1u << loc;
    set(board, loc, piece);
    taken &= ~(1u << piece);
}

static void
unset_piece(u8 loc, u8 piece) {
    u8 x = X(loc);
    u8 y = Y(loc);
    cols[x]--;
    rows[y]--;
    if (x == y) diag[0]--;
    if (x == 3 - y) diag[1]--;
    filled &= ~(1u << loc);
    unset(board, loc);
    taken |= 1u << piece;
}

static u8
cell_wins_xy(u8 x, u8 y) {
    u8 ret = 0;
    if (rows[y] == 4) {
        ret |=
            boardAt(0, y) &
            boardAt(1, y) &
            boardAt(2, y) &
            boardAt(3, y);
        ret |=
            ~(boardAt(0, y) |
              boardAt(1, y) |
              boardAt(2, y) |
              boardAt(3, y)) & 0xFu;
    }
    if (cols[x] == 4) {
        ret |=
            boardAt(x, 0) &
            boardAt(x, 1) &
            boardAt(x, 2) &
            boardAt(x, 3);
        ret |=
            ~(boardAt(x, 0) |
              boardAt(x, 1) |
              boardAt(x, 2) |
              boardAt(x, 3)) & 0xFu;
    }
    if (x == y && diag[0] == 4) {
        ret |=
            boardAt(0, 0) &
            boardAt(1, 1) &
            boardAt(2, 2) &
            boardAt(3, 3);
        ret |=
            ~(boardAt(0, 0) |
              boardAt(1, 1) |
              boardAt(2, 2) |
              boardAt(3, 3)) & 0xFu;
    }
    if (x == 3 - y && diag[1] == 4) {
        ret |=
            boardAt(0, 3) &
            boardAt(1, 2) &
            boardAt(2, 1) &
            boardAt(3, 0);
        ret |=
            ~(boardAt(0, 3) |
              boardAt(1, 2) |
              boardAt(2, 1) |
              boardAt(3, 0)) & 0xFu;
    }
    return ret;
}

static u8
cell_wins(u8 loc) {
    return cell_wins_xy(loc % 4, loc / 4);
}

static void
print_board(FILE *out) {
    for (u8 y = 0; y < 4; y++) {
        for (u8 x = 0; x < 4; x++) {
            if (isFilled(4u * y + x)) {
                char binary[4];
                print_piece(boardAt(x, y), binary, cell_wins_xy(x, y));
                fprintf(out, "[%.*s] ", 4, binary);
            } else {
                fprintf(out, "[    ] ");
            }
        }
        fprintf(out, "\n");
    }
}

static int
score(u8 piece, int a, int b, u8 depth, u8 *bestSpot, u8 *bestPiece) {
    u8 found = 0, numSpots = 0, numPieces = 0, spots[16], nextPieces[16];
    for (u8 spot = 0; spot < 16; spot++) {
        if (!isFilled(spot)) {
            spots[numSpots++] = spot;
        }
    }
    for (u8 nextPiece = 0; nextPiece < 16; nextPiece++) {
        if (nextPiece != piece && !isTaken(nextPiece)) {
            nextPieces[numPieces++] = nextPiece;
        }
    }
    shuffle(nextPieces, numPieces, sizeof(*nextPieces), rand);
    shuffle(spots, numSpots, sizeof(*spots), rand);
    *bestPiece = nextPieces[0];
    *bestSpot = spots[0];
    //Check if any moves win outright
    for (u8 i = 0; i < numSpots; i++) {
        u8 spot = spots[i];
        set_piece(spot, piece);
        if (cell_wins(spot)) {
            *bestSpot = spot;
            unset_piece(spot, piece);
            return MAX_SCORE;
        }
        unset_piece(spot, piece);
    }
    //Check if the only move ties the game
    if (numSpots == 1) {
        return 0;
    }
    int num_mistakes = 0, maxScore = -INT_MAX;
    for (u8 i = 0; i < numSpots; i++) {
        u8 spot = spots[i];
        set_piece(spot, piece);
        for (u8 j = 0; !found && j < numPieces; j++) {
            u8 nextPiece = nextPieces[j];
            u8 nextBestSpot, nextBestPiece;
            int s = -score(nextPiece, -b, -a, depth + 1,
                           &nextBestSpot, &nextBestPiece);
            if (s > maxScore) {
                maxScore = s;
                *bestPiece = nextPiece;
                *bestSpot = spot;
            }
            if (s < -(MAX_SCORE / 2)) {
                num_mistakes++;
            }
            a = max(a, s);
            if (b <= a) {
                found = 1;
            }
        }
        unset_piece(spot, piece);
        if (found) break;
    }
    return maxScore - num_mistakes;
}

u8
prompt_piece() {
    u8 piece;
    while (1) {
        int c[4] = {0};
        printf("Enter piece:\n");
        for (int i = 0; i < 4; i++) {
            printf("(%c)%s/(%c)%s?\n", pieces[i][0][0], pieces[i][0] + 1,
                   pieces[i][1][0], pieces[i][1] + 1);
        }
        printf("> ");
        for (int i = 0; i < 4; i++) {
            c[i] = getchar();
        }
        int failed = 0;
        piece = 0;
        for (u8 i = 0; i < 4; i++) {
            if (c[i] == pieces[i][0][0]) {
                continue;
            } else if (c[i] == pieces[i][1][0]) {
                piece |= 1u << i;
            } else {
                failed = 1;
                break;
            }
        }
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {
            failed = 1;
        }
        if (!failed && isTaken(piece)) {
            printf("Piece already taken.\n");
            failed = 1;
        }
        if (!failed) break;
        printf("Invalid input.\n");
    }
    return piece;
}

u8
prompt_move(int *done) {
    int p[2];
    while (1) {
        printf("Enter position (0-3):\n");
        printf("xy?\n");
        printf("> ");
        if ((p[0] = getchar()) == '\n') {
            *done = 1;
            return 0;
        };
        p[1] = getchar();
        int failed = 0;
        for (int i = 0; i < 2; i++) {
            if ('0' <= p[i] && p[i] <= '3') {
                p[i] -= '0';
                continue;
            } else {
                failed = 1;
                break;
            }
        }
        if (!failed && isFilled(4u * p[1] + p[0])) {
            printf("Location already taken.\n");
            failed = 1;
        }
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {
            failed = 1;
        }
        if (!failed) break;
        printf("Invalid input.\n");
    }
    return 4 * p[1] + p[0];
}

int
main() {
    time_t seed = time(NULL);
    srand(seed);
    printf("seed: %ld\n", seed);
    for (int i = 0; i < 7; i++) {
        u8 piece;
        do {
            piece = rand() % 16;
        } while (isTaken(piece));
        u8 loc;
        do {
            loc = rand() % 16;
        } while (isFilled(loc));
        set_piece(loc, piece);
        if (cell_wins(loc)) {
            unset_piece(loc, piece);
            i--;
        }
    }
    print_board(stdout);
    u8 piece;
    while(1) {
        do {
            piece = rand() % 16;
        } while (isTaken(piece));
        int valid = 1;
        for (u8 i = 0; valid && i < 16; i++) {
            if (!isFilled(i)) {
                set_piece(i, piece);
                if (cell_wins(i)) {
                    valid = 0;
                }
                unset_piece(i, piece);
            }
        }
        if (valid) break;
    }
    clock_t t = clock();
    u8 nextSpot, nextPiece;
    while(taken) {
        printf("Available pieces: ");
        char *sep = "";
        for (u8 i = 0; i < 16; i++) {
            if (!isTaken(i)) {
                char str[5] = {0};
                print_piece(i, str, 0);
                printf("%s%s", sep, str);
                sep = ", ";
            }
        }
        printf("\n");
        char binPiece[9] = {0};
        print_piece(piece, binPiece, 0);
        printf("Next piece: %s = 0x%02X\n", binPiece, piece);
        int s = score(piece, -INT_MAX, INT_MAX, 0, &nextSpot, &nextPiece);
        printf("Next spot: (%d, %d) = %d\n", nextSpot%4, nextSpot/4, nextSpot);
        set_piece(nextSpot, piece);
        piece = nextPiece;
        print_board(stdout);
        printf("SCORE: %d\n", s);
        printf("----------------\n");
        if (cell_wins(nextSpot)) break;
    }
    t = clock() - t;
    double time_taken = ((double)t)/CLOCKS_PER_SEC;
    printf("Time taken: %lf seconds\n", time_taken);
    return 0;
}