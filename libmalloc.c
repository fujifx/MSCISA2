#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include "ptrListRow.h"

/* macros & global variables */
#define listSize 16777216             /* number of items in freeList array */
#define alignment 32                  /* all pointers for memory have to be aligned to this value */
#define canary (uint64_t)0xabcdefab   /* recongisable canary block contents */
#define canarySize 8                  /* the number of bytes either side of the writable memory block */

struct ptrListRow ptrList[listSize];  /* stores meta data for pointer allocation; doubles as the free list */

/* given an address, moves it to align with 32 bytes */
void* alignPtr(void *addr) {
  int offset = (uintptr_t)addr % alignment;
  int padding = (alignment - (offset % alignment)) % alignment;
  return sbrk(padding);
}

/* given an address (to a free block), ensure every byte from that address to address+size is clean */
void verifyCleanBlock(void *addr, size_t blockSize) {
  int index;              /* for-loop index */

  for(index = 0; index < blockSize; index++) {
    if(*(char *)(addr+index) != '\0') {
      printf("x\n");
      abort();
    }
  }

  return;
}

/* given an address (to any block), ensure every byte in the left and right red-spaces are clean */
void verifyCanaryIntegrity(void *addr, size_t blockSize) {
  uint64_t *left = addr - canarySize;
  uint64_t *right = addr + blockSize;

  if(*left != canary) {
    printf("l\n");
    abort();
  }

  if(*right != canary) {
    printf("r\n");
    abort();
  }

  return;
}

/* sets red-space as canary; called by malloc & realloc */
void initializeCanaries(void *addr, size_t blockSize) {
  uint64_t *left = addr - canarySize;
  uint64_t *right = addr + blockSize;
  *left  = canary;
  *right = canary;
  return;
}

/* given a size of memory wanted, will return a pointer to a block of memory to start writing in */
void* malloc(size_t size) {
  int index, temp;
  size_t allocSize;
  void* addr = NULL;

  /* if size is 0, return NULL */
  if(size == 0) {
    return NULL;
  }

  /* adjust size to also include redSpace */
  allocSize = size + 2*canarySize;

  /* if a small chunk... */
  if( (allocSize > 0) && (allocSize <= 2048) ) {
    /* set chunkSize to me a multiple of 2 in size */
    for(index = 64; index <= 2048; index = index*2) {
      if (allocSize < index) {
        allocSize = index;
      }

      if(allocSize == index) {
        break;
      }
    }

    /* look for chunk to reuse */
    for(index = 0; index < listSize; index++) {
      if( (ptrList[index].pointer != NULL) && (ptrList[index].isFree == 1) && (ptrList[index].chunkSize == allocSize) ) {

        ptrList[index].isFree = 0;
        addr = ptrList[index].pointer;
      }
    }

    /* if failed to find a block to reuse, make a new block */
    if(addr == NULL) {
      addr = sbrk(0) + canarySize;
      sbrk(allocSize);
      initializeCanaries(addr, size);

      for(index = 0; index < listSize; index++) {
        if(ptrList[index].pointer == NULL) {
          ptrList[index].pointer = addr;
          ptrList[index].blockSize = size;
          ptrList[index].chunkSize = allocSize;
          ptrList[index].isFree = 0;
          break;
        }
      }
    }

    return addr;
  }
  /* else if a large chunk... */
  else if (allocSize > 2048) {
    /* set chunkSize to me a multiple of 4096 in size */
    temp = allocSize % 4096;
    if(temp != 0) {
      allocSize = allocSize + (4096-temp);
    }

    /* look for a chunk to reuse */
    for(index = 0; index < listSize; index++) {
      if( (ptrList[index].pointer != NULL) && (ptrList[index].isFree == 1) && (ptrList[index].chunkSize == allocSize) ) {

        ptrList[index].isFree = 0;
        addr = ptrList[index].pointer;
      }
    }

    /* if failed to find a block to reuse, make a new block */
    if(addr==NULL) {
      addr = mmap(NULL, allocSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0) + 8;
      initializeCanaries(addr, size);

      for(index = 0; index < listSize; index++) {
        if(ptrList[index].pointer == NULL) {
          ptrList[index].pointer = addr;
          ptrList[index].blockSize = size;
          ptrList[index].chunkSize = allocSize;
          ptrList[index].isFree = 0;
          break;
        }
      }
    }

    return addr;
  }
  /* else, this was invalid and return null */
  else {
    return NULL;
  }

}

/* given a pointer managed by ptrList, sets that pointer to be free (and ready for reuse), it also sets the memory to be completely blank */
void free(void* ptr) {
  int index;

  if(ptr != NULL) {
    for(index = 0; index < listSize; index++) {
      if(ptrList[index].pointer == ptr) {
        if(ptrList[index].isFree == 0) {
          ptrList[index].isFree = 1;
          verifyCanaryIntegrity(ptr, ptrList[index].blockSize);
          explicit_bzero(ptr, ptrList[index].blockSize);

          return;
        }
        else {
          abort();
        }
      }
    }

    abort();
  }

  return;
}

/* given num elements of size size, will return a pointer to a block of memory with size num*size AND initialize all that space to 0s */
void* calloc(size_t num, size_t size) {
  int index;                  /* for-loop index */
  void* addr = NULL;          /* pointer to be returned */

  if( (size == 0) || (num == 0) ){
    return NULL;
  }

  addr = malloc(num*size);

  if(addr != NULL) {
    for(index = 0; index < listSize; index++) {
      if(ptrList[index].pointer == addr) {
        explicit_bzero(addr, ptrList[index].blockSize);
        break;
      }
    }
  }

  return addr;
}

/* given a size of memory wanted, will return a pointer to a block of memory to start writing in  */
void* realloc(void* ptr, size_t new_size) {
  int index, old_size;        /* for-loop index and original size */
  void* addr = NULL;          /* pointer to be returned */

  if(ptr==NULL) {
    return malloc(new_size);
  }

  if(new_size == 0) {
    free(ptr);
    return NULL;
  }

  old_size = 0;
  for(index = 0; index < listSize; index++) {
    if(ptrList[index].pointer == ptr) {
      break;
    }
  }

  if( (new_size+2*canarySize) > ptrList[index].chunkSize) {
    addr = malloc(new_size);
    memcpy(addr, ptr, old_size);
    free(ptr);
  }
  else {
    ptrList[index].blockSize = new_size;
    initializeCanaries(ptr, new_size);
  }

  return addr;
}

/* checks that all currently used pointers red-space is intact and all unused blocks are clean */
void ItSS_HeapTest() {
  int index;

  for(index = 0; index < listSize; index++) {
    if(ptrList[index].pointer != NULL) {
      if(ptrList[index].isFree == 1) {
        verifyCleanBlock(ptrList[index].pointer, ptrList[index].blockSize);
      }
      else {
        verifyCanaryIntegrity(ptrList[index].pointer, ptrList[index].blockSize);
      }
    }
  }

  return;
}
