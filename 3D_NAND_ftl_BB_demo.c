#include <stdio.h>

#define BLOCKS 4
#define WORDLINES 4
#define BITLINES 8
#define MAX_LBA 8

#define MAX_CELL_VALUE 7

typedef struct {
    int cells[BITLINES];
    int valid;   // valid page
} WordLine;

typedef struct {
    WordLine wl[WORDLINES];
    int is_erased;
    int is_bad;
} Block;

typedef struct {
    int block;
    int wl;
} PBA;

typedef struct {
    Block blocks[BLOCKS];
    PBA lba_map[MAX_LBA];
} NAND;

// Initialize
void init_nand(NAND *n) {
    for (int b = 0; b < BLOCKS; b++) {
        n->blocks[b].is_erased = 1;
        n->blocks[b].is_bad = 0;

        for (int w = 0; w < WORDLINES; w++) {
            n->blocks[b].wl[w].valid = 0;
            for (int bl = 0; bl < BITLINES; bl++) {
                n->blocks[b].wl[w].cells[bl] = -1;
            }
        }
    }

    // Factory bad block
    n->blocks[1].is_bad = 1;

    // Init mapping
    for (int i = 0; i < MAX_LBA; i++) {
        n->lba_map[i].block = -1;
        n->lba_map[i].wl = -1;
    }
}

// Find free WL
int find_free_page(NAND *n, int *block, int *wl) {
    for (int b = 0; b < BLOCKS; b++) {
        if (n->blocks[b].is_bad) continue;

        for (int w = 0; w < WORDLINES; w++) {
            if (n->blocks[b].wl[w].valid == 0) {
                *block = b;
                *wl = w;
                return 0;
            }
        }
    }
    return -1;
}

// Write (FTL)
void ftl_write(NAND *n, int lba, int *data) {
    int block, wl;

    if (find_free_page(n, &block, &wl) != 0) {
        printf("No free page! (GC needed)\n");
        return;
    }

    printf("Write LBA %d -> Block %d WL %d\n", lba, block, wl);

    // Invalidate old mapping
    if (n->lba_map[lba].block != -1) {
        int old_b = n->lba_map[lba].block;
        int old_w = n->lba_map[lba].wl;

        n->blocks[old_b].wl[old_w].valid = 0;
        printf("Invalidate old PBA B%d WL%d\n", old_b, old_w);
    }

    // Program new location
    for (int i = 0; i < BITLINES; i++) {
        n->blocks[block].wl[wl].cells[i] = data[i];
    }
    n->blocks[block].wl[wl].valid = 1;

    // Update mapping
    n->lba_map[lba].block = block;
    n->lba_map[lba].wl = wl;
}

// Read (FTL)
void ftl_read(NAND *n, int lba) {
    PBA p = n->lba_map[lba];

    if (p.block == -1) {
        printf("LBA %d not written\n", lba);
        return;
    }

    printf("Read LBA %d -> Block %d WL %d:\n", lba, p.block, p.wl);

    for (int i = 0; i < BITLINES; i++) {
        printf("%d ", n->blocks[p.block].wl[p.wl].cells[i]);
    }
    printf("\n");
}

// Demo
int main() {
    NAND nand;
    init_nand(&nand);

    int data1[BITLINES] = {1,1,1,1,1,1,1,1};
    int data2[BITLINES] = {2,2,2,2,2,2,2,2};
    int data3[BITLINES] = {2,3,2,3,2,3,2,3};

    ftl_write(&nand, 0, data1);
    ftl_read(&nand, 0);

    // overwrite same LBA
    ftl_write(&nand, 0, data2);
    ftl_read(&nand, 0);
    
        // overwrite same LBA
    ftl_write(&nand, 0, data3);
    ftl_read(&nand, 0);


    return 0;
}
