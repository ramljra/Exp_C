#include <stdio.h>
#include <stdlib.h>

#define BLOCKS 4
#define WORDLINES 4
#define BITLINES 8

#define MAX_CELL_VALUE 7

typedef struct {
    int cells[BITLINES];
} WordLine;

typedef struct {
    WordLine wl[WORDLINES];
    int is_erased;
    int is_bad;     // NEW: bad block flag
} Block;

typedef struct {
    Block blocks[BLOCKS];
} NAND;

// Initialize NAND
void init_nand(NAND *n) {
    for (int b = 0; b < BLOCKS; b++) {
        n->blocks[b].is_erased = 1;
        n->blocks[b].is_bad = 0;

        for (int w = 0; w < WORDLINES; w++) {
            for (int bl = 0; bl < BITLINES; bl++) {
                n->blocks[b].wl[w].cells[bl] = -1;
            }
        }
    }

    // Simulate factory bad blocks
    n->blocks[1].is_bad = 1;
    printf("Factory Bad Block: 1\n");
}

// Find next good block
int find_good_block(NAND *n) {
    for (int b = 0; b < BLOCKS; b++) {
        if (!n->blocks[b].is_bad) {
            return b;
        }
    }
    return -1;
}

// Erase block
void erase_block(NAND *n, int block) {
    if (n->blocks[block].is_bad) {
        printf("ERROR: Block %d is BAD!\n", block);
        return;
    }

    printf("Erase Block %d\n", block);

    for (int w = 0; w < WORDLINES; w++) {
        for (int bl = 0; bl < BITLINES; bl++) {
            n->blocks[block].wl[w].cells[bl] = -1;
        }
    }
    n->blocks[block].is_erased = 1;
}

// Program wordline
void program_wordline(NAND *n, int block, int wl, int *data) {
    if (n->blocks[block].is_bad) {
        printf("ERROR: Block %d is BAD. Finding replacement...\n", block);

        int new_block = find_good_block(n);
        if (new_block == -1) {
            printf("No good blocks available!\n");
            return;
        }

        printf("Redirecting to Block %d\n", new_block);
        block = new_block;
    }

    if (!n->blocks[block].is_erased) {
        printf("ERROR: Block not erased!\n");
        return;
    }

    printf("Program Block %d WL %d\n", block, wl);

    for (int bl = 0; bl < BITLINES; bl++) {
        if (data[bl] > MAX_CELL_VALUE) {
            printf("Invalid value!\n");
            return;
        }
        n->blocks[block].wl[wl].cells[bl] = data[bl];
    }

    n->blocks[block].is_erased = 0;
}

// Read wordline
void read_wordline(NAND *n, int block, int wl) {
    if (n->blocks[block].is_bad) {
        printf("ERROR: Cannot read BAD block %d\n", block);
        return;
    }

    printf("Read Block %d WL %d:\n", block, wl);

    for (int bl = 0; bl < BITLINES; bl++) {
        printf("%d ", n->blocks[block].wl[wl].cells[bl]);
    }
    printf("\n");
}

// Mark block as bad (runtime failure)
void mark_bad_block(NAND *n, int block) {
    printf("Marking Block %d as BAD (runtime)\n", block);
    n->blocks[block].is_bad = 1;
}

// Demo
int main() {
    NAND nand;
    init_nand(&nand);

    int data[BITLINES] = {0,1,2,3,4,5,6,7};

    // Try writing to a bad block
    program_wordline(&nand, 1, 0, data);

    // Normal write
    program_wordline(&nand, 0, 0, data);
    read_wordline(&nand, 0, 0);

    // Mark block bad during runtime
    mark_bad_block(&nand, 0);

    // Try using it again
    program_wordline(&nand, 0, 1, data);

    return 0;
}
