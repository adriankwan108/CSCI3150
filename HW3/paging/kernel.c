#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kernel.h"

// The simple min function you can use in your codes.
#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

int KERNEL_SPACE_SIZE = 8192;
int VIRTUAL_SPACE_SIZE = 512;
int PAGE_SIZE = 32;
int MAX_PROCESS_NUM = 8;

// The kernel managed memory content is set to 0 initiallly.
struct Kernel * init_kernel() {
	struct Kernel * kernel = (struct Kernel *)malloc(sizeof(struct Kernel));

	kernel->space = (char *)malloc(sizeof(char) * KERNEL_SPACE_SIZE);
	kernel->allocated_pages = 0;
	kernel->occupied_pages = (char *)malloc(sizeof(char) * KERNEL_SPACE_SIZE / PAGE_SIZE);
	kernel->running = (char *)malloc(sizeof(char) * MAX_PROCESS_NUM);
	kernel->mm = (struct MMStruct *)malloc(sizeof(struct MMStruct) * MAX_PROCESS_NUM);

	for(int i = 0; i < MAX_PROCESS_NUM; i ++)
		kernel->mm[i].page_table = NULL;

	memset(kernel->space, 0, sizeof(char) * KERNEL_SPACE_SIZE);
	memset(kernel->occupied_pages, 0, sizeof(char) * KERNEL_SPACE_SIZE / PAGE_SIZE);
	memset(kernel->running, 0, sizeof(char) * MAX_PROCESS_NUM);

	return kernel;
}

void destroy_kernel(struct Kernel * kernel) {
	free(kernel->space);
	free(kernel->occupied_pages);
	free(kernel->running);
	for(int i = 0; i < MAX_PROCESS_NUM; i ++){
		if(kernel->mm[i].page_table != NULL)
			free(kernel->mm[i].page_table);
	}
	free(kernel->mm);
	free(kernel);
}

void print_kernel_free_space(struct Kernel * kernel) {
	int idx = 0;
	char * addr = kernel->space;
	printf("free space: ");
	while(idx < KERNEL_SPACE_SIZE / PAGE_SIZE) {
		while(idx < KERNEL_SPACE_SIZE / PAGE_SIZE && kernel->occupied_pages[idx] == 1) {
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

void get_kernel_free_space_info(struct Kernel * kernel, char * buf) {
	int i = sprintf(buf, "free space: ");
	int idx = 0;
	char * addr = kernel->space;
	while(idx < KERNEL_SPACE_SIZE / PAGE_SIZE) {
		while(idx < KERNEL_SPACE_SIZE / PAGE_SIZE && kernel->occupied_pages[idx] == 1) {
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

void print_memory_mappings(struct Kernel * kernel, int pid) {
	if(kernel->running[pid] == 0) {
		printf("The process is not running\n");
	}
	else {
		printf("Memory mappings of process %d\n", pid);
		for(int i = 0; i < (kernel->mm[pid].size + PAGE_SIZE - 1) / PAGE_SIZE; i++) {
			if(kernel->mm[pid].page_table[i].present == 0)
				printf("virtual page %d: Not present\n", i);
			else
				printf("virtual page %d -> physical page %d\n", i, kernel->mm[pid].page_table[i].PFN);
		}
	}
	printf("\n");
}

/*
	1. Check if a free process slot exists and if the there's enough free space (check allocated_pages).
	2. Alloc space for page_table (the size of it depends on how many pages you need) and update allocated_pages.
	3. The mapping to kernel-managed memory is not built up, all the PFN should be set to -1 and present byte to 0.
	4. Return a pid (the index in MMStruct array) which is >= 0 when success, -1 when failure in any above step.
*/
int proc_create_vm(struct Kernel * kernel, int size) {
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

	if((kernel->allocated_pages + page_need)> (KERNEL_SPACE_SIZE/PAGE_SIZE))
	{
		return -1; //check if there is enough free space
	}
	
	int i;
	for(i = 0; i < MAX_PROCESS_NUM; i++){
		if(kernel->running[i] == 0) //check if a free process slot exists
		{
			//exists
			kernel->allocated_pages += page_need; //update allocated_pages
			kernel->running[i] = 1; //update running

			//allocate spaces for page table

			kernel->mm[i].page_table = (struct PTE*)malloc(sizeof(struct PTE)* (kernel->allocated_pages));
			kernel->mm[i].size = size;

			//memset all  PFN to be -1 and present byte to 0
			for(int j=0; j<kernel->allocated_pages;j++)
			{
				kernel->mm[i].page_table[j].PFN = -1;
				kernel->mm[i].page_table[j].present = 0;
			}
			return i; //return pid
		}
	}
	return -1;

	
}

/*
	This function will read the range [addr, addr+size) from user space of a specific process to the buf (buf should be >= size).
	1. Check if the reading range is out-of-bounds.
	2. If the pages in the range [addr, addr+size) of the user space of that process are not present,
	   you should firstly map them to the free kernel-managed memory pages (first fit policy).
	Return 0 when success, -1 when failure (out of bounds).
*/
int vm_read(struct Kernel * kernel, int pid, char * addr, int size, char * buf) {
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
			for(int j=0; j< (KERNEL_SPACE_SIZE / PAGE_SIZE) ; j++)
			{
				if(kernel->occupied_pages[j] == 0)
				{
					kernel->occupied_pages[j] = 1;  
					kernel->mm[pid].page_table[i].PFN = j;  //set the page_table
					break;
				}				
			}
			kernel->mm[pid].page_table[i].present = 1;
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
	1. Check if the writing range is out-of-bounds.
	2. If the pages in the range [addr, addr+size) of the user space of that process are not present,
	   you should firstly map them to the free kernel-managed memory pages (first fit policy).
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
			for(int j=0; j< (KERNEL_SPACE_SIZE / PAGE_SIZE) ; j++)
			{
				if(kernel->occupied_pages[j] == 0)
				{
					kernel->occupied_pages[j] = 1;  
					kernel->mm[pid].page_table[i].PFN = j;  //set the page_table
					break;
				}				
			}
			kernel->mm[pid].page_table[i].present = 1;
		}
	}

	memcpy(kernel->space + (uintptr_t)(addr), buf, size);
	return  0;
}

/*
	This function will free the space of a process.
	1. Reset the corresponding pages in occupied_pages to 0.
	2. Release the page_table in the corresponding MMStruct and set to NULL.
	Return 0 when success, -1 when failure.
*/
int proc_exit_vm(struct Kernel * kernel, int pid){
	// Fill your codes below.
	if(pid > MAX_PROCESS_NUM || kernel->running[pid] == 0)
		return -1;

	for(int i=0; i<kernel->mm[pid].size/PAGE_SIZE;i++)
	{
		//check present = 1;
		//take the PFN;
		if(kernel->mm[pid].page_table[i].present==1)
		{
			kernel->occupied_pages[kernel->mm[pid].page_table[i].PFN]=0;
		}
	}
	free(kernel->mm[pid].page_table);
	return 0;
}
