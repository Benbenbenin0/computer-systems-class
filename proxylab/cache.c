#include "cache.h"

cache cache_new ()
{
    cache C = malloc(sizeof(struct cache_header));
	  C->items = vector_new ();
	  C->size = 0;
	  return C;
}
void cache_free (cache C) 
{
    vector_free (C->items);
    free (C);
}

const char *cache_get (cache C, char *website, char *file, int port, 
    int *data_size)
{
	  vector V = C->items;
	  web_data w = vector_get (V, website, file, port);
	  
    if (w != NULL) 
    {
        *data_size = w->data_size;
		    return w->data;
	  }
    
    else 
    {
        *data_size = 0;
		    return NULL;
    }
}

void cache_insert (cache C, char *website, char *file, int port, 
    char *data, int dataSize)
{
	  vector V = C->items;
	  // Get the web_data pointer which we will insert into the memory
	  web_data w = web_data_new (website, file, port, data, dataSize);
	  // while the size of the cache is bigger than 1 Mb
	  
    while (C->size + dataSize > MAX_CACHE_SIZE) 
    {
		    C->size -= vector_evict_LRU (V);
	  }
	  
    vector_push_back (V, w);
    C->size += dataSize;
}
