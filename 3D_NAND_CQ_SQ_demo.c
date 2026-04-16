#include <stdio.h>
#include <string.h>

#define BLOCKS 4
#define WORDLINES 4
#define BITLINES 8
#define MAX_LBA 8
#define QDEPTH 8

#define NVME_WRITE 0
#define NVME_READ  1

// ---------------- NAND STRUCTURES ----------------

typedef struct {
    int cells[BITLINES];
    int valid;
} WordLine;

typedef struct {
    WordLine wl[WORDLINES];
    int is_bad;
    int erase_count;
} Block;

typedef struct {
    int block;
    int wl;
} PBA;

typedef struct {
    Block blocks[BLOCKS];
    PBA lba_map[MAX_LBA];
} NAND;

NAND nand;

// ---------------- NVMe STRUCTURES ----------------

typedef struct {
    int opcode;
    int lba;
    int *prp;  // host buffer
} NVMeCmd;

NVMeCmd SQ[QDEPTH];
int CQ[QDEPTH];

int sq_head = 0, sq_tail = 0;
int cq_head = 0, cq_tail = 0;

// ---------------- INIT ----------------

void init_nand() {
    for (int b = 0; b < BLOCKS; b++) {
        nand.blocks[b].is_bad = 0;
        nand.blocks[b].erase_count = 0;

        for (int w = 0; w < WORDLINES; w++) {
            nand.blocks[b].wl[w].valid = 0;
            for (int i = 0; i < BITLINES; i++)
                nand.blocks[b].wl[w].cells[i] = -1;
        }
    }

    // Factory bad block
    nand.blocks[1].is_bad = 1;

    for (int i = 0; i < MAX_LBA; i++) {
        nand.lba_map[i].block = -1;
        nand.lba_map[i].wl = -1;
    }
}

// ---------------- FTL HELPERS ----------------

int find_free_page(int *block, int *wl) {
    int min_erase = 9999;

    for (int b = 0; b < BLOCKS; b++) {
        if (nand.blocks[b].is_bad) continue;

        if (nand.blocks[b].erase_count > min_erase)
            continue;

        for (int w = 0; w < WORDLINES; w++) {
            if (nand.blocks[b].wl[w].valid == 0) {
                *block = b;
                *wl = w;
                min_erase = nand.blocks[b].erase_count;
                break;
            }
        }
    }

    return (*block == -1) ? -1 : 0;
}

int select_victim_block() {
    int max_invalid = -1, victim = -1;

    for (int b = 0; b < BLOCKS; b++) {
        if (nand.blocks[b].is_bad) continue;

        int invalid = 0;
        for (int w = 0; w < WORDLINES; w++)
            if (nand.blocks[b].wl[w].valid == 0)
                invalid++;

        if (invalid > max_invalid) {
            max_invalid = invalid;
            victim = b;
        }
    }
    return victim;
}

// ---------------- GC ----------------

void garbage_collect() {
    int victim = select_victim_block();

    if (victim == -1) return;

    printf("GC: Victim Block %d\n", victim);

    for (int w = 0; w < WORDLINES; w++) {
        if (nand.blocks[victim].wl[w].valid) {

            int new_b = -1, new_w = -1;

            if (find_free_page(&new_b, &new_w) != 0)
                return;

            // Copy data
            for (int i = 0; i < BITLINES; i++)
                nand.blocks[new_b].wl[new_w].cells[i] =
                    nand.blocks[victim].wl[w].cells[i];

            nand.blocks[new_b].wl[new_w].valid = 1;

            // Update mapping
            for (int l = 0; l < MAX_LBA; l++) {
                if (nand.lba_map[l].block == victim &&
                    nand.lba_map[l].wl == w) {

                    nand.lba_map[l].block = new_b;
                    nand.lba_map[l].wl = new_w;
                }
            }
        }
    }

    // Erase block
    for (int w = 0; w < WORDLINES; w++) {
        nand.blocks[victim].wl[w].valid = 0;
        for (int i = 0; i < BITLINES; i++)
            nand.blocks[victim].wl[w].cells[i] = -1;
    }

    nand.blocks[victim].erase_count++;
}

// ---------------- FTL ----------------

void ftl_write(int lba, int *data) {
    int block = -1, wl = -1;

    if (find_free_page(&block, &wl) != 0) {
        printf("FTL: Trigger GC\n");
        garbage_collect();

        if (find_free_page(&block, &wl) != 0) {
            printf("FTL: No space\n");
            return;
        }
    }

    printf("FTL WRITE: LBA %d → B%d WL%d\n", lba, block, wl);

    // Invalidate old
    if (nand.lba_map[lba].block != -1) {
        int ob = nand.lba_map[lba].block;
        int ow = nand.lba_map[lba].wl;
        nand.blocks[ob].wl[ow].valid = 0;
    }

    // Write
    for (int i = 0; i < BITLINES; i++)
        nand.blocks[block].wl[wl].cells[i] = data[i];

    nand.blocks[block].wl[wl].valid = 1;

    nand.lba_map[lba].block = block;
    nand.lba_map[lba].wl = wl;
}

void ftl_read(int lba, int *data) {
    int b = nand.lba_map[lba].block;
    int w = nand.lba_map[lba].wl;

    if (b == -1) {
        printf("FTL READ: Unmapped LBA\n");
        return;
    }

    for (int i = 0; i < BITLINES; i++)
        data[i] = nand.blocks[b].wl[w].cells[i];

    printf("FTL READ: LBA %d → B%d WL%d\n", lba, b, w);
}

// ---------------- NVMe ----------------

void submit_cmd(NVMeCmd cmd) {
    SQ[sq_tail] = cmd;
    sq_tail = (sq_tail + 1) % QDEPTH;

    printf("Host: Submit opcode=%d LBA=%d\n", cmd.opcode, cmd.lba);
}

void post_completion(int status) {
    CQ[cq_tail] = status;
    cq_tail = (cq_tail + 1) % QDEPTH;
}

void process_sq() {
    while (sq_head != sq_tail) {

        NVMeCmd *cmd = &SQ[sq_head];

        printf("Controller: Process opcode=%d LBA=%d\n",
               cmd->opcode, cmd->lba);

        if (cmd->opcode == NVME_WRITE)
            ftl_write(cmd->lba, cmd->prp);

        else if (cmd->opcode == NVME_READ)
            ftl_read(cmd->lba, cmd->prp);

        post_completion(0);

        sq_head = (sq_head + 1) % QDEPTH;
    }
}

void poll_cq() {
    while (cq_head != cq_tail) {
        printf("Host: Completion = %d\n", CQ[cq_head]);
        cq_head = (cq_head + 1) % QDEPTH;
    }
}

// ---------------- MAIN ----------------

int main() {

    init_nand();

    int write_buf[BITLINES] = {1,2,3,4,5,6,7,8};
    int read_buf[BITLINES] = {0};

    NVMeCmd w = {NVME_WRITE, 0, write_buf};
    NVMeCmd r = {NVME_READ, 0, read_buf};

    submit_cmd(w);
    submit_cmd(r);

    process_sq();
    poll_cq();

    printf("Read Data: ");
    for (int i = 0; i < BITLINES; i++)
        printf("%d ", read_buf[i]);

    printf("\n");

    return 0;
}
