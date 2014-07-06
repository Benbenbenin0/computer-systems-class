#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>
#include <assert.h>
#include "web_data.h"

struct vector_header 
{
    int size;
    int capacity;
    web_data* arr; // array of webdata
};
typedef struct vector_header *vector;

vector vector_new ();
void vector_free (vector V);

web_data vector_get (vector V, char *website, char *file, int port);
void vector_push_back (vector V, web_data w);
int vector_evict_LRU (vector V);

#endif
