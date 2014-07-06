#ifndef CACHE_H
#define CACHE_H

#include <stdlib.h>
#include "vector.h"

#define MAX_CACHE_SIZE 1049000

struct cache_header
{
	vector items;
	int size;
};
typedef struct cache_header *cache;

cache cache_new ();
void cache_free (cache C);

const char *cache_get (cache C, char *website, char *file, int port, 
    int *data_size);
void cache_insert (cache C, char *website, char *file, int port, 
    char *data, int dataSize);

#endif
