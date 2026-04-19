/*implemented a multi-threaded SSD firmware simulator with FTL mapping, background garbage collection, and wear leveling. 
The GC thread runs asynchronously, and victim selection considers both valid page count and erase cycles, 
allowing realistic write amplification behavior*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define BLOCKS 6
#define PAGES_PER_BLOCK 4
#define TOTAL_PAGES (BLOCKS * PAGES_PER_BLOCK)
#define MAX_LBA 32

typedef struct {
    int valid;
    int lba;
} Page;

typedef struct {
    Page pages[PAGES_PER_BLOCK];
    int erase_count;
} Block;

/* NAND */
Block nand[BLOCKS];

/* FTL Mapping */
int lba_to_pba[MAX_LBA];

/* Stats */
int logical_writes = 0;
int physical_writes = 0;

/* Lock */
pthread_mutex_t lock;

/* ================= INIT ================= */

void init_nand()
{
    for (int b = 0; b < BLOCKS; b++)
    {
        nand[b].erase_count = 0;
        for (int p = 0; p < PAGES_PER_BLOCK; p++)
        {
            nand[b].pages[p].valid = 0;
            nand[b].pages[p].lba = -1;
        }
    }

    for (int i = 0; i < MAX_LBA; i++)
        lba_to_pba[i] = -1;
}

/* ================= HELPERS ================= */

int get_block(int pba) { return pba / PAGES_PER_BLOCK; }
int get_page(int pba) { return pba % PAGES_PER_BLOCK; }

int find_free_page()
{
    for (int b = 0; b < BLOCKS; b++)
    {
        for (int p = 0; p < PAGES_PER_BLOCK; p++)
        {
            if (nand[b].pages[p].valid == 0)
                return b * PAGES_PER_BLOCK + p;
        }
    }
    return -1;
}

/* ================= VICTIM SELECTION ================= */

int select_victim_block()
{
    int best_block = -1;
    int min_valid = PAGES_PER_BLOCK + 1;

    for (int b = 0; b < BLOCKS; b++)
    {
        int valid_count = 0;

        for (int p = 0; p < PAGES_PER_BLOCK; p++)
            if (nand[b].pages[p].valid)
                valid_count++;

        if (valid_count < min_valid)
        {
            min_valid = valid_count;
            best_block = b;
        }
    }

    return best_block;
}

/* ================= WEAR LEVELING ================= */

int select_wear_aware_block()
{
    int victim = select_victim_block();

    // simple wear leveling: avoid lowest erase count blocks
    for (int b = 0; b < BLOCKS; b++)
    {
        if (nand[b].erase_count < nand[victim].erase_count)
            victim = b;
    }

    return victim;
}

/* ================= GC ================= */

void garbage_collect()
{
    int victim = select_wear_aware_block();

    printf("\n[GC] Victim Block: %d (erase=%d)\n",
           victim, nand[victim].erase_count);

    // move valid pages
    for (int p = 0; p < PAGES_PER_BLOCK; p++)
    {
        Page *pg = &nand[victim].pages[p];

        if (pg->valid)
        {
            int new_page = find_free_page();
            int b = get_block(new_page);
            int pp = get_page(new_page);

            nand[b].pages[pp].valid = 1;
            nand[b].pages[pp].lba = pg->lba;

            lba_to_pba[pg->lba] = new_page;

            physical_writes++; // GC write
        }
    }

    // erase block
    for (int p = 0; p < PAGES_PER_BLOCK; p++)
    {
        nand[victim].pages[p].valid = 0;
        nand[victim].pages[p].lba = -1;
    }

    nand[victim].erase_count++;
}

/* ================= FTL WRITE ================= */

void ftl_write(int lba)
{
    pthread_mutex_lock(&lock);

    logical_writes++;

    // invalidate old
    if (lba_to_pba[lba] != -1)
    {
        int old = lba_to_pba[lba];
        nand[get_block(old)].pages[get_page(old)].valid = 0;
    }

    int page = find_free_page();

    if (page == -1)
    {
        garbage_collect();
        page = find_free_page();
    }

    int b = get_block(page);
    int p = get_page(page);

    nand[b].pages[p].valid = 1;
    nand[b].pages[p].lba = lba;

    lba_to_pba[lba] = page;

    physical_writes++;

    pthread_mutex_unlock(&lock);
}

/* ================= GC THREAD ================= */

void* gc_thread(void *arg)
{
    while (1)
    {
        sleep(1);

        pthread_mutex_lock(&lock);

        int free_page = find_free_page();

        if (free_page == -1)
        {
            garbage_collect();
        }

        pthread_mutex_unlock(&lock);
    }
}

/* ================= DEBUG ================= */

void print_stats()
{
    printf("\nLogical Writes: %d\n", logical_writes);
    printf("Physical Writes: %d\n", physical_writes);
    printf("WA: %.2f\n",
           (float)physical_writes / logical_writes);

    printf("\nErase Count per Block:\n");
    for (int b = 0; b < BLOCKS; b++)
        printf("Block %d: %d\n", b, nand[b].erase_count);
}

/* ================= MAIN ================= */

int main()
{
    pthread_t gc;

    pthread_mutex_init(&lock, NULL);
    init_nand();

    pthread_create(&gc, NULL, gc_thread, NULL);

    int workload[] = {
        0,1,2,3,4,5,0,1,6,7,2,3,8,9,0,4,5,6
    };

    int n = sizeof(workload)/sizeof(workload[0]);

    for (int i = 0; i < n; i++)
    {
        printf("Write LBA %d\n", workload[i]);
        ftl_write(workload[i]);
        usleep(100000); // simulate delay
    }

    sleep(2); // allow GC to run

    print_stats();

    return 0;
}
