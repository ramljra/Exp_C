#include <stdio.h>
#include <string.h>

#define BLOCKS 2
#define PAGES_PER_BLOCK 4
#define CELLS_PER_PAGE 8   // small for demo

// Simulate QLC (4 bits per cell → values 0–15)
#define MAX_CELL_VALUE 16

typedef struct {
    int cells[CELLS_PER_PAGE];
} Page;

typedef struct {
    Page pages[PAGES_PER_BLOCK];
    int is_erased;
} Block;

typedef struct {
    Block blocks[BLOCKS];
} Plane;

typedef struct {
    Plane plane;
} Die;

// Initialize NAND
void init_nand(Die *die) {
    for (int b = 0; b < BLOCKS; b++) {
        die->plane.blocks[b].is_erased = 1;
        for (int p = 0; p < PAGES_PER_BLOCK; p++) {
            for (int c = 0; c < CELLS_PER_PAGE; c++) {
                die->plane.blocks[b].pages[p].cells[c] = -1; // empty
            }
        }
    }
}

// Erase block (set all cells to empty)
void erase_block(Die *die, int block) {
    printf("Erasing Block %d...\n", block);
    for (int p = 0; p < PAGES_PER_BLOCK; p++) {
        for (int c = 0; c < CELLS_PER_PAGE; c++) {
            die->plane.blocks[block].pages[p].cells[c] = -1;
        }
    }
    die->plane.blocks[block].is_erased = 1;
}

// Write page (only if erased or empty)
void write_page(Die *die, int block, int page, int *data) {
    if (!die->plane.blocks[block].is_erased) {
        printf("ERROR: Block not erased!\n");
        return;
    }

    printf("Writing Block %d Page %d\n", block, page);

    for (int i = 0; i < CELLS_PER_PAGE; i++) {
        if (data[i] > MAX_CELL_VALUE) {
            printf("Invalid cell value!\n");
            return;
        }
        die->plane.blocks[block].pages[page].cells[i] = data[i];
    }

    die->plane.blocks[block].is_erased = 0;
}

// Read page
void read_page(Die *die, int block, int page) {
    printf("Reading Block %d Page %d:\n", block, page);

    for (int i = 0; i < CELLS_PER_PAGE; i++) {
        printf("%d ", die->plane.blocks[block].pages[page].cells[i]);
    }
    printf("\n");
}

// Demo
int main() {
    Die nand;
    init_nand(&nand);

    int data[CELLS_PER_PAGE] = {1,2,3,4,5,6,7,0};
    int data1[CELLS_PER_PAGE] = {9,10,11,12,13,14,15,15};

    write_page(&nand, 0, 0, data);   // should work
    read_page(&nand, 0, 0);

    write_page(&nand, 0, 1, data);   // should fail (no erase)
    
    //erase_block(&nand, 0);
    read_page(&nand, 0, 0);
    erase_block(&nand, 1);
    write_page(&nand, 1, 1, data1);   // now works
    read_page(&nand, 1, 1);

    return 0;
}
