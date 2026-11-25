#ifndef __PTRLISTROW_H__
#define __PTRLISTROW_H__

#include <stdlib.h>

struct ptrListRow {
  void*  pointer;
  int    isFree;
  size_t blockSize; /* size requested */
  size_t chunkSize; /* size of memory allocated */
};

#endif // __PTRLISTROW_H__
