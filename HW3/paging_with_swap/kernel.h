extern int KERNEL_SPACE_SIZE;
extern int VIRTUAL_SPACE_SIZE;
extern int PAGE_SIZE;
extern int MAX_PROCESS_NUM;

// For simplicity, instead of storing the physical page id, we store the virtual page here.
// As a result, our LRU will help you manage the page mapping and page swap together.
struct LRUEntry {
        int pid;
        int virtual_page_id;
        struct LRUEntry * next;
        struct LRUEntry * prev;
};

struct LRU {
        int num_entries; // Number of entries in the LRU.
        struct LRUEntry * head;
        struct LRUEntry * tail;
};

/*
        Reference: textbook chapter 18.
        To make it simple, we do not encode PFN and flag bits together to an integer.
        PTE: page table entry.
        PFN: page frame number.
                (1). The page id in kernel-managed memory if present=1.
                (2). The page id in swap file if present=0.

        present:
                (1). present = 0
                        (1.1). If PFN != -1, it means the page is in the swap file.
                        (1.2). If PFN == -1, it means the memory mapping for this page is not yet built.
        dirty: represents if the page in kernel-managed memory is dirty (has been written by some process), 0 -> not dirty, 1 -> dirty. 

        Currently when the pages are allocated (proc_create_vm), present will be set to 0 and present will be set to 0
        because the translation is not yet built.
        After you access this page (vm_read && vm_write), you will need to either
                (1) build the translation and present will be set to 1 if the page is currently not present.
                (2) swap-in the page from swap file if the page is currently present.
*/
struct PTE {
        int PFN;
        char present;
        char dirty;
};

/*
        1. The user space and the user space page id start from 0.
        2. size indicates the size of user space (&& kernel-managed memory) allocated for this process.
        3. page_table is an array of PTE (page table entry).
*/ 
struct MMStruct {
        int size;
        struct PTE * page_table;
};

struct SwapInfoStruct {
        // An array to indicate the page occupation in swap file, 0 for free, 1 for occupied.
        // Size = number of swap file pages;
        char * swap_map;
        // An array to map kernel-managed memory pages to swap file pages.
        // When the element = -1, it means the mapping is not built.
        // Size = number of kernel-managed memory pages.
        int * swapper_space;
};

struct Kernel {
        char * space;
        char * occupied_pages;      // For simplicity, we use a char array to indicate the free pages, 0 for free, 1 for occupied.
        struct SwapInfoStruct * si; // The manager for swap space.
        char * running;             // An array marking if the process is running.
        struct MMStruct * mm;       // An array of MMStruct for each process.
        struct LRU lru;
};

struct Kernel * init_kernel();
void destroy_kernel(struct Kernel * kernel);
void print_kernel_free_space(struct Kernel * kernel);                // Print the free kernel space.
void print_kernel_lru(struct Kernel * kernel);                       // Print lru information.
void get_kernel_free_space_info(struct Kernel * kernel, char * buf); // Copy the free kernel free space information to buf.
void get_kernel_lru_info(struct Kernel * kernel, char * buf);        // Copy lru information to buf.
void print_memory_mappings(struct Kernel * kernel, int pid);         // Print memory mappings for a specific process.

// Pop the head of the LRU.
void lru_del(struct Kernel * kernel);

// Add an page to LRU (pass the pid and the virtual page id).
//      1. If the entry is already in the LRU, move it to the tail.
//      2. If the entry is not in the LRU, append it to the tail.
void lru_add(struct Kernel * kernel, int pid, int virtual_page_id);

/*
        1. Check if there's a not-occupied process slot.
        2. Alloc space for page_table (the size of it depends on how many pages you need).
        3. The mapping to kernel-managed memory is not built, present bit and dirty bit are set to 0, PFN is set to -1.
        Return a pid (the index in MMStruct array) which is >= 0 when success, -1 when failure.
*/
int proc_create_vm(struct Kernel * kernel, int size);

/*
        This function will read the range [addr, addr+size) from user space of a specific process to the buf (buf should be >= size).
        1. Check if the pid is valid and if the reading range is out-of-bounds.
        2. Map the pages in the range [addr, addr+size) of the user space of that process to kernel-managed memory using lru_add.
        3. Read the content.
        Return 0 when success, -1 when failure (out of bounds).
*/
int vm_read(struct Kernel * kernel, int pid, char * addr, int size, char * buf);

/*
        This function will write the content of buf to user space [addr, addr+size) (buf should be >= size).
        1. Check if the pid is valid and if the writing range is out-of-bounds.
        2. Map the pages in the range [addr, addr+size) of the user space of that process to kernel-managed memory using lru_add.
        3. Write the content.
        Return 0 when success, -1 when failure (out of bounds).
*/
int vm_write(struct Kernel * kernel, int pid, char * addr, int size, char * buf);

/*
        1. Check if the pid is valid.
        2. Iterate the LRU queue to delete those active pages of this process.
        3. Iterate the page_table in MMStruct.
                3.1. Update occupied_pages and swapper_space if present=1.
                3.2. Update swap_map if present=0 and PFN!=-1.
        Return 0 when success, -1 when failure.
*/
int proc_exit_vm(struct Kernel * kernel, int pid);