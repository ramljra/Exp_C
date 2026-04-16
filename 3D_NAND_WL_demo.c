#include <stdio.h>

#define BLOCKS 2
#define WORDLINES 4      // pages per block
#define BITLINES 8       // cells per wordline (columns)

#define MAX_CELL_VALUE 7   // TLC (3 bits)

// WordLine = Page
typedef struct {
    int cells[BITLINES];   // cells across bitlines
} WordLine;

typedef struct {
    WordLine wl[WORDLINES];
    int is_erased;
} Block;

typedef struct {
    Block blocks[BLOCKS];
} NAND;

// Initialize
void init_nand(NAND *n) {
    for (int b = 0; b < BLOCKS; b++) {
        n->blocks[b].is_erased = 1;
        for (int w = 0; w < WORDLINES; w++) {
            for (int bl = 0; bl < BITLINES; bl++) {
                n->blocks[b].wl[w].cells[bl] = -1;
            }
        }
    }
}

// Erase block
void erase_block(NAND *n, int block) {
    printf("Erase Block %d\n", block);
    for (int w = 0; w < WORDLINES; w++) {
        for (int bl = 0; bl < BITLINES; bl++) {
            n->blocks[block].wl[w].cells[bl] = -1;
        }
    }
    n->blocks[block].is_erased = 1;
}

// Program one word line (page)
void program_wordline(NAND *n, int block, int wl, int *data) {
    if (!n->blocks[block].is_erased) {
        printf("ERROR: Block not erased!\n");
        return;
    }

    printf("Program Block %d WL %d\n", block, wl);

    for (int bl = 0; bl < BITLINES; bl++) {
        if (data[bl] > MAX_CELL_VALUE) {
            printf("Invalid value at BL %d\n", bl);
            return;
        }
        n->blocks[block].wl[wl].cells[bl] = data[bl];
    }

    n->blocks[block].is_erased = 0;
}

// Read word line
void read_wordline(NAND *n, int block, int wl) {
    printf("Read Block %d WL %d:\n", block, wl);

    for (int bl = 0; bl < BITLINES; bl++) {
        printf("%d ", n->blocks[block].wl[wl].cells[bl]);
    }
    printf("\n");
}

// Partial programming (important in NAND)
void partial_program(NAND *n, int block, int wl, int bl, int value) {
    printf("Partial Program: B%d WL%d BL%d = %d\n", block, wl, bl, value);

    if (value > MAX_CELL_VALUE) {
        printf("Invalid value!\n");
        return;
    }

    n->blocks[block].wl[wl].cells[bl] = value;
}

// Demo
int main() {
    NAND nand;
    init_nand(&nand);

    int data[BITLINES] = {0,1,2,3,4,5,6,7};

    program_wordline(&nand, 0, 0, data);
    read_wordline(&nand, 0, 0);

    // Partial programming example
    partial_program(&nand, 0, 0, 3, 6);
    read_wordline(&nand, 0, 0);

    erase_block(&nand, 0);

    program_wordline(&nand, 0, 1, data);
    read_wordline(&nand, 0, 1);

    return 0;
}
