#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kernel.h"

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

int KERNEL_SPACE_SIZE = 256;
int VIRTUAL_SPACE_SIZE = 512;
int PAGE_SIZE = 32;
int MAX_PROCESS_NUM = 8;

struct Kernel * init_kernel(){
	struct Kernel * kernel = (struct Kernel *)malloc(sizeof(struct Kernel));

	kernel->space = (char *)malloc(sizeof(char) * KERNEL_SPACE_SIZE);
	kernel->occupied_pages = (char *)malloc(sizeof(char) * KERNEL_SPACE_SIZE / PAGE_SIZE);
	kernel->si = (struct SwapInfoStruct *)malloc(sizeof(struct SwapInfoStruct));
	kernel->running = (char *)malloc(sizeof(char) * MAX_PROCESS_NUM);
	kernel->mm = (struct MMStruct *)malloc(sizeof(struct MMStruct) * MAX_PROCESS_NUM);

	// Initialize the memory mappings manager for each process.
	for(int i = 0; i < MAX_PROCESS_NUM; i ++) {
		kernel->mm[i].page_table = NULL;
	}

	// Initialize the swap area manager.
	kernel->si->swap_map = (char *)malloc(sizeof(char) * MAX_PROCESS_NUM * VIRTUAL_SPACE_SIZE / PAGE_SIZE);
	kernel->si->swapper_space = (int *)malloc(sizeof(int) * KERNEL_SPACE_SIZE / PAGE_SIZE);
	memset(kernel->si->swap_map, 0, sizeof(char) * MAX_PROCESS_NUM * VIRTUAL_SPACE_SIZE / PAGE_SIZE);
	for(int i = 0; i < KERNEL_SPACE_SIZE / PAGE_SIZE; i++) {
		kernel->si->swapper_space[i] = -1;
	}

	kernel->lru.num_entries = 0;
	kernel->lru.head = NULL;
	kernel->lru.tail = NULL;

	memset(kernel->space, 0, sizeof(char) * KERNEL_SPACE_SIZE);
	memset(kernel->occupied_pages, 0, KERNEL_SPACE_SIZE / PAGE_SIZE);
	memset(kernel->running, 0, MAX_PROCESS_NUM);

	// Create swap file and fill the content with 0.
	FILE * f = fopen("swap", "wb");
	if(f == NULL) {
		printf("error opening swap in init_kernel\n");
		exit(-1);
	}
	for(int j = 0; j < VIRTUAL_SPACE_SIZE * MAX_PROCESS_NUM; j ++)
		putc(0, f);
	fclose(f);

	return kernel;
}

void destroy_kernel(struct Kernel * kernel){
	while(kernel->lru.num_entries != 0)
		lru_del(kernel);

	free(kernel->space);
	free(kernel->occupied_pages);
	free(kernel->running);
	for(int i = 0; i < MAX_PROCESS_NUM; i ++){
		if(kernel->mm[i].page_table != NULL)
			free(kernel->mm[i].page_table);
	}
	free(kernel->mm);
	free(kernel->si->swap_map);
	free(kernel->si->swapper_space);
	free(kernel->si);
	free(kernel);
}

void print_kernel_free_space(struct Kernel * kernel){
	int idx = 0;
	char * addr = kernel->space;
	printf("free space: ");
	while(idx < KERNEL_SPACE_SIZE / PAGE_SIZE){
		while(idx < KERNEL_SPACE_SIZE / PAGE_SIZE && kernel->occupied_pages[idx] == 1){
			++ idx;
			addr += PAGE_SIZE;
		}
		int last = idx;
		while(idx < KERNEL_SPACE_SIZE / PAGE_SIZE && kernel->occupied_pages[idx] == 0)
			++ idx;
		if(idx < KERNEL_SPACE_SIZE / PAGE_SIZE)
			printf("(addr:%d, size:%d) -> ", (int)(addr - kernel->space), (idx - last) * PAGE_SIZE);
		else
			printf("(addr:%d, size:%d)\n", (int)(addr - kernel->space), (idx - last) * PAGE_SIZE);
		addr += PAGE_SIZE * (idx - last);
	}
}

void get_kernel_free_space_info(struct Kernel * kernel, char * buf){
	int i = sprintf(buf, "free space: ");
	int idx = 0;
	char * addr = kernel->space;
	while(idx < KERNEL_SPACE_SIZE / PAGE_SIZE){
		while(idx < KERNEL_SPACE_SIZE / PAGE_SIZE && kernel->occupied_pages[idx] == 1){
			++ idx;
			addr += PAGE_SIZE;
		}
		int last = idx;
		while(idx < KERNEL_SPACE_SIZE / PAGE_SIZE && kernel->occupied_pages[idx] == 0)
			++ idx;
		int n;
		if(idx < KERNEL_SPACE_SIZE / PAGE_SIZE)
			n = sprintf(buf + i, "(addr:%d, size:%d) -> ", (int)(addr - kernel->space), (idx - last) * PAGE_SIZE);
		else
			n = sprintf(buf + i, "(addr:%d, size:%d)\n", (int)(addr - kernel->space), (idx - last) * PAGE_SIZE);
		i += n;
		addr += PAGE_SIZE * (idx - last);
	}
}

void print_kernel_lru(struct Kernel * kernel){
	printf("Kernel LRU: ");
	if(kernel->lru.num_entries == 0) {
		printf("\n");
		return;
	}
	struct LRUEntry * temp = kernel->lru.head;
	while(temp != NULL){
		if(temp->next == NULL)
			printf("(pid:%d, page:%d)\n", temp->pid, temp->virtual_page_id);
		else
			printf("(pid:%d, page:%d) -> ", temp->pid, temp->virtual_page_id);
		temp = temp->next;
	}
}

void get_kernel_lru_info(struct Kernel * kernel, char * buf){
	int i = 0;
	if(kernel->lru.num_entries == 0)
		return;
	struct LRUEntry * temp = kernel->lru.head;
	while(temp != NULL){
		if(temp->next == NULL){
			int n = sprintf(buf + i, "(pid:%d, page:%d)", temp->pid, temp->virtual_page_id);
			i += n;
		}
		else{
			int n = sprintf(buf + i, "(pid:%d, page:%d) -> ", temp->pid, temp->virtual_page_id);
			i += n;
		}
		temp = temp->next;
	}
}

void print_memory_mappings(struct Kernel * kernel, int pid){
	if(kernel->running[pid] == 0) {
		printf("The process is not running\n");
	}
	else {
		printf("Memory mappings of process %d\n", pid);
		for(int i = 0; i < (kernel->mm[pid].size + PAGE_SIZE - 1) / PAGE_SIZE; i++) {
			if(kernel->mm[pid].page_table[i].present == 0) {
				if(kernel->mm[pid].page_table[i].PFN == -1)
					printf("virtual page %d: Not present\n", i);
				else
					printf("virtual page %d -> swap file page %d\n", i, kernel->mm[pid].page_table[i].PFN);
			}
			else
				printf("virtual page %d -> physical page %d\n", i, kernel->mm[pid].page_table[i].PFN);
		}
	}
	printf("\n");
}

void lru_del(struct Kernel * kernel){
	if(kernel->lru.num_entries != 0){
		int pid = kernel->lru.head->pid;
		int virtual_page_id = kernel->lru.head->virtual_page_id;
		int pfn = kernel->mm[pid].page_table[virtual_page_id].PFN;

		int swap_page_id;
		// If the page is dirty, write to the swap file.
		if(kernel->mm[pid].page_table[virtual_page_id].dirty == 1){
			// Need to persist the page back to disk.
			FILE * f = fopen("swap", "r+");
			if(f == NULL) {
				printf("error opening swap in lru_del\n");
				exit(-1);
			}

			// Find the swap file page to write.
			swap_page_id = kernel->si->swapper_space[pfn];
			if(swap_page_id == -1) {
				// If this pfn is not a swapped-in page, find a new not-occupied swap file page.
				swap_page_id = 0;
				while(kernel->si->swap_map[swap_page_id] == 1)
					swap_page_id++;
				kernel->si->swap_map[swap_page_id] = 1;
			}

			// Set the write position.
			fseek(f, swap_page_id * PAGE_SIZE, SEEK_SET);
			fwrite(kernel->space + PAGE_SIZE * pfn, sizeof(char), sizeof(char) * PAGE_SIZE, f);
			memset(kernel->space + PAGE_SIZE * pfn, 0, PAGE_SIZE);
			fclose(f);
		}
		else{
			swap_page_id = kernel->si->swapper_space[pfn];
			if(swap_page_id == -1) {
				// If this pfn is not a swapped-in page, find a new not-occupied swap file page.
				swap_page_id = 0;
				while(kernel->si->swap_map[swap_page_id] == 1)
					swap_page_id++;
				kernel->si->swap_map[swap_page_id] = 1;
				FILE * f = fopen("swap", "r+");
				if(f == NULL) {
					printf("error opening swap in lru_del\n");
					exit(-1);
				}
				fseek(f, swap_page_id * PAGE_SIZE, SEEK_SET);
				fwrite(kernel->space + PAGE_SIZE * pfn, sizeof(char), sizeof(char) * PAGE_SIZE, f);
				memset(kernel->space + PAGE_SIZE * pfn, 0, PAGE_SIZE);
				fclose(f);
			}
		}

		kernel->occupied_pages[pfn] = 0; // Release the occupied page.

		kernel->mm[pid].page_table[virtual_page_id].present = 0;
		kernel->mm[pid].page_table[virtual_page_id].dirty = 0;
		kernel->mm[pid].page_table[virtual_page_id].PFN = swap_page_id; // Map to swap file page id.
		
		// Delete the head of the LRU queue.
		struct LRUEntry * temp = kernel->lru.head;
		if(temp->next == NULL){
			kernel->lru.head = NULL;
			kernel->lru.tail = NULL;
		}
		else{
			kernel->lru.head = temp->next;
			kernel->lru.head->prev = NULL;
		}
		kernel->lru.num_entries -= 1;
		free(temp);
	}
}

// Add an entry to LRU.
// 	1. If the entry is already in the LRU, move it to the tail.
//	2. If the entry is not in the LRU, append it to the tail.
void lru_add(struct Kernel * kernel, int pid, int virtual_page_id){
	// First to find if the entry exists.
	if(kernel->lru.num_entries > 0){
		struct LRUEntry * cur = kernel->lru.head;
		while(cur != NULL){
			if(cur->pid == pid && cur->virtual_page_id == virtual_page_id){
				if(kernel->lru.num_entries == 1 || cur->next == NULL)
					return;

				if(cur->prev == NULL){
					kernel->lru.head = cur->next;
					kernel->lru.head->prev = NULL;
				}
				else
					cur->prev->next = cur->next;
				cur->next->prev = cur->prev;
				kernel->lru.tail->next = cur;
				cur->next = NULL;
				cur->prev = kernel->lru.tail;
				kernel->lru.tail = cur;
				return;
			}
			cur = cur->next;
		}
	}

	// If LRU is full, pop one entry.
	if(kernel->lru.num_entries >= KERNEL_SPACE_SIZE / PAGE_SIZE)
		lru_del(kernel);
	for(int i = 0; i < KERNEL_SPACE_SIZE / PAGE_SIZE; i ++){
		if(kernel->occupied_pages[i] == 0){
			if(kernel->mm[pid].page_table[virtual_page_id].present == 0 && kernel->mm[pid].page_table[virtual_page_id].PFN != -1) {
				// The page is in the swap file.
				FILE * f = fopen("swap", "r+");
				if(f == NULL) {
					printf("error opening swap in lru_add\n");
					exit(-1);
				}
				fseek(f, kernel->mm[pid].page_table[virtual_page_id].PFN * PAGE_SIZE, SEEK_SET);
				fread(kernel->space + PAGE_SIZE * i, sizeof(char), sizeof(char) * PAGE_SIZE, f);
				fclose(f);

				// Update SwapInfoStruct (map PFN to the swapped-in page).
				kernel->si->swapper_space[i] = kernel->mm[pid].page_table[virtual_page_id].PFN;
			}
			else {
				// The mapping has not yet been built.
				kernel->mm[pid].page_table[virtual_page_id].present = 1;
			}

			kernel->mm[pid].page_table[virtual_page_id].PFN = i;
			kernel->mm[pid].page_table[virtual_page_id].present = 1;

			kernel->occupied_pages[i] = 1;

			// Append the new entry to the tail of the LRU.
			struct LRUEntry * temp = (struct LRUEntry *)malloc(sizeof(struct LRUEntry));
			temp->pid = pid;
			temp->virtual_page_id = virtual_page_id;
			temp->next = NULL;
			if(kernel->lru.tail != NULL){
				temp->prev = kernel->lru.tail;
				kernel->lru.tail->next = temp;
				kernel->lru.tail = temp;
			}
			else{
				kernel->lru.head = temp;
				kernel->lru.tail = temp;
				kernel->lru.head->prev = NULL;
				kernel->lru.head->next = NULL;
				kernel->lru.tail->prev = NULL;
				kernel->lru.tail->next = NULL;
			}
			kernel->lru.num_entries += 1;

			break;
		}
	}
}

/*
        1. Check if there's a not-occupied process slot.
        2. Alloc space for page_table (the size of it depends on how many pages you need).
        3. The mapping to kernel-managed memory is not built, present bit and dirty bit are set to 0, PFN is set to -1.
        Return a pid (the index in MMStruct array) which is >= 0 when success, -1 when failure.
*/
int proc_create_vm(struct Kernel * kernel, int size){
	if(size > VIRTUAL_SPACE_SIZE) //check if the process larger than limit
	{
		return -1;
	}

	int page_need;
	page_need = size / PAGE_SIZE;
	if((size - (PAGE_SIZE * page_need))!=0)
	{
		page_need ++;
	}
	
	int i;
	for(i = 0; i < MAX_PROCESS_NUM; i++){
		if(kernel->running[i] == 0) //check if a free process slot exists
		{
			//exists
			kernel->running[i] = 1; //update running

			//allocate spaces for page table

			kernel->mm[i].page_table = (struct PTE*)malloc(sizeof(struct PTE)* (page_need));
			kernel->mm[i].size = size;

			//memset all  PFN to be -1 and present byte to 0
			for(int j=0; j<page_need;j++)
			{
				kernel->mm[i].page_table[j].PFN = -1;
				kernel->mm[i].page_table[j].present = 0;
				kernel->mm[i].page_table[j].dirty = 0;
			}
			return i; //return pid
		}
	}
	return -1;
}

/*
        This function will read the range [addr, addr+size) from user space of a specific process to the buf (buf should be >= size).
        1. Check if the pid is valid and if the reading range is out-of-bounds.
        2. Map the pages in the range [addr, addr+size) of the user space of that process to kernel-managed memory using lru_add.
        3. Read the content.
        Return 0 when success, -1 when failure (out of bounds).
*/
int vm_read(struct Kernel * kernel, int pid, char * addr, int size, char * buf){
	if(pid > MAX_PROCESS_NUM || kernel->running[pid] == 0 || (uintptr_t)(addr) + (uintptr_t)(size) > (uintptr_t)(kernel->mm[pid].size))
		return -1;
	
	int fake_addr = (long)addr/PAGE_SIZE;
	int final_pos = (size+(long)addr+1)/PAGE_SIZE;

	//printf("fake_addr = %d\n", fake_addr);
	//printf("final_pos = %d\n\n", final_pos);
	for(int i = fake_addr; i < final_pos+1; i++)
	{
		//for i=fake_address to number of page_table
		if(kernel->mm[pid].page_table[i].present == 0)
		{
			//map them until found the free kernel-managed memory pages
			for(int j=0; j< (MAX_PROCESS_NUM*VIRTUAL_SPACE_SIZE/PAGE_SIZE);j++)
			{
				if(kernel->occupied_pages[j] == 0)
				{
					kernel->occupied_pages[j] = 1;  
					kernel->mm[pid].page_table[i].PFN = j;  //set the page_table
					break;
				}
				else if(kernel->occupied_pages[j] == 1)
				{
					for(int k=0;k < KERNEL_SPACE_SIZE / PAGE_SIZE;k++)
					{
						if(kernel->si->swapper_space[k]==-1)
						{
							kernel->si->swapper_space[k]==0;
							kernel->si->swap_map[pid]=k;
							lru_add(kernel, pid, j);
							break;
						}
					}
				}
			}
			kernel->mm[pid].page_table[i].present = 1;

		}
		else if(kernel->mm[pid].page_table[i].present == 1)
		{
			//swap in
			lru_del(kernel);
			kernel->si->swap_map[pid]=0;
			kernel->mm[pid].page_table[i].present = 0;
		}
	}
	//printf("size before = %d\n", size);
	//size = (final_pos-fake_addr+1)*PAGE_SIZE;
	//printf("size after = %d\n\n", size);
	memcpy(buf, kernel->space + (uintptr_t)(addr), size); //don't know if correct
	return 0;
}

/*
        This function will write the content of buf to user space [addr, addr+size) (buf should be >= size).
        1. Check if the pid is valid and if the writing range is out-of-bounds.
        2. Map the pages in the range [addr, addr+size) of the user space of that process to kernel-managed memory using lru_add.
        3. Write the content.
        Return 0 when success, -1 when failure (out of bounds).
*/
int vm_write(struct Kernel * kernel, int pid, char * addr, int size, char * buf){
	if(pid > MAX_PROCESS_NUM || kernel->running[pid] == 0 || (uintptr_t)(addr) + (uintptr_t)(size) > (uintptr_t)(kernel->mm[pid].size))
		return -1;


	int fake_addr = (long)(addr)/PAGE_SIZE;
	int final_pos = (size+(long)addr+1)/PAGE_SIZE;

	//printf("fake_addr = %d\n", fake_addr);
	//printf("final_pos = %d\n\n", final_pos);
	for(int i = fake_addr; i < final_pos; i++)
	{
		//for i=fake_address to number of page_table
		if(kernel->mm[pid].page_table[i].present == 0)
		{
			//map them until found the free kernel-managed memory pages
			for(int j=0; j< (MAX_PROCESS_NUM*VIRTUAL_SPACE_SIZE/PAGE_SIZE);j++)
			{
				if(kernel->occupied_pages[j] == 0)
				{
					kernel->occupied_pages[j] = 1;  
					kernel->mm[pid].page_table[i].PFN = j;  //set the page_table
					break;
				}
				else if(kernel->occupied_pages[j] == 1)
				{
					for(int k=0;k < KERNEL_SPACE_SIZE / PAGE_SIZE;k++)
					{
						if(kernel->si->swapper_space[k]==-1)
						{
							kernel->si->swapper_space[k]==0;
							kernel->si->swap_map[pid]=k;
							lru_add(kernel, pid, j);
							break;
						}
					}
				}
			}
			kernel->mm[pid].page_table[i].present = 1;
		}
	}

	memcpy(kernel->space + (uintptr_t)(addr), buf, size);
	return  0;
}

/*
        1. Check if the pid is valid.
        2. Iterate the LRU queue to delete those active pages of this process.
        3. Iterate the page_table in MMStruct.
                3.1. Update occupied_pages and swapper_space if present=1.
                3.2. Update swap_map if present=0 and PFN!=-1.
        Return 0 when success, -1 when failure.
*/
int proc_exit_vm(struct Kernel * kernel, int pid){
	if(pid > MAX_PROCESS_NUM || kernel->running[pid] == 0)
		return -1;

	lru_del(kernel);
	kernel->si->swap_map[kernel->si->swapper_space[pid]]=0;
	for(int i=0; i<kernel->mm[pid].size/PAGE_SIZE;i++)
	{
		//check present = 1;
		//take the PFN;
		if(kernel->mm[pid].page_table[i].present==1)
		{
			kernel->occupied_pages[kernel->mm[pid].page_table[i].PFN]=0;
			//update swapper_space
			kernel->si->swapper_space[pid]=-1 ;
		}
	}
	free(kernel->mm[pid].page_table);
	return 0;
}
