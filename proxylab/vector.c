#include "vector.h"

void vector_pop (vector V, int index);
int vector_find_LRU (vector V);
void vector_resize (vector V);

/*
 * vector_new - constructor for vector
 */
vector vector_new () 
{
    vector V = malloc(sizeof(struct vector_header));
    V->size = 0;
    V->capacity = 5;
    V->arr = calloc(V->capacity, sizeof(web_data));
    return V;
}

/*
 * vector_free - frees a vector and all its contents
 */
void vector_free (vector V) 
{
    // Free elements in array
    int i;
    for (i = 0; i < V->size; i ++) 
    {
        web_data_free (V->arr[i]);
    }
    // Free the array
    free (V->arr);
    // Free V
    free (V);
}

/*
 * vector_get - Return web_data_hdr pointer (i.e. web_data) that matches
 * website, file, and port
 */
web_data vector_get (vector V, char *website, char *file, int port)
{
    // Iterate over the vector
    int i;
    for (i = 0; i < V->size; i ++) {
        // If matches
        if (web_data_equals (V->arr[i], website, file, port)) {
            // Update the used timestamp and return
            web_data_update_acc_time (V->arr[i]);
            return V->arr[i];
        }
    }
    // Not found
    return NULL;
}

/* 
 * vector_pop -  pops the element at index in the vector
 */
void vector_pop (vector V, int index) 
{
    assert (index >= 0);
    assert (index < V->size);
    
    // Dumb way: shift every pointer after 'index' one spot back in the array
    int i;
    for (i = index + 1; i < V->size; i ++) {
        V->arr[i - 1] = V->arr[i];
    }
    V->size = V->size - 1;
}

/*
 * vector_find_LRU - Returns index of least recently used element 
 * in the vector
 */
int vector_find_LRU (vector V) 
{
    assert (V->size > 0);
    // Index of the least recently used element
    int best_index = 0;
    // Iterate over the vector
    int i;
    for (i = 1; i < V->size; i ++) {
        if (V->arr[i]->acc_time < V->arr[best_index]->acc_time) {
            best_index = i;
        }
    }
    return best_index;
}

/*
 * vector_evict_LRU - Evicts the least recently used element in the vector
 * Returns size of data removed
 */
int vector_evict_LRU (vector V) 
{
    assert (V->size > 0);
    // Get the index
    int index = vector_find_LRU (V);
    // size of data removed
    int size = V->arr[index]->data_size;
    // free the data stored in the cache
    web_data_free(V->arr[index]);
    // pop from the list of pointers
    vector_pop (V, index);  
    return size;
}

/*
 * vector_push_back - push a pointer of type w into the vector
 */
void vector_push_back (vector V, web_data w)
{
    // If vector is filled to capacity, increase the capacity
    if (V->size == V->capacity)
        vector_resize (V);

    // Insert into the vector
    V->arr[V->size] = w;
    V->size = V->size + 1;
}

/*
 * vector_resize - double the capacity of the vector
 */
void vector_resize (vector V) {
    V->capacity = V->capacity * 2;
    
    // create new array
    web_data* new_arr = calloc(V->capacity, sizeof(web_data));
    // copy elements over
    int i;
    for (i = 0; i < V->size; i ++) {
        new_arr[i] = V->arr[i];
    }

    // free old array
    free (V->arr);

    // point array to the new array
    V->arr = new_arr;
}
