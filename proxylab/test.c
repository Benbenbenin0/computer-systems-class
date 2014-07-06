/* This function tests the cache data structure */

#include <assert.h>
#include <stdio.h>
#include <time.h>
#include "cache.h"

/*
 * stall - stall the program so that clock() will actually register ticks
 * pretty overkill but it works 
 */
void stall (int n) {
  int i,j;
  int freq=n-1;
  for (i=2; i<=n; ++i) for (j=i - 1;j>1;--j) if (i%j==0) {--freq; break;}
  return;
}

int main () {
    // Allocate local variables
    int *dummy = malloc(sizeof(int));
    cache C = cache_new ();

    // Init some macros
    int BIG = 500000;
    char *large_string = malloc (BIG);
    memset (large_string, 49, BIG);

    // Insert google.com
    assert (cache_get (C, "www.google.com", "/", 80, dummy) == NULL);
    cache_insert (C, "www.google.com", "/", 80, large_string, BIG);
    assert (cache_get (C, "www.google.com", "/", 80, dummy) != NULL);
    assert (cache_get (C, "www.google.com", "/", 80, dummy) != NULL);
    assert (cache_get (C, "www.google.com", "/", 80, dummy) != NULL);
    assert (cache_get (C, "www.google.com", "/", 80, dummy) != NULL);
    stall (9999);

    // Insert youtube.com
    assert (cache_get (C, "www.youtube.com", "/", 80, dummy) == NULL);
    cache_insert (C, "www.youtube.com", "/", 80, large_string, BIG);
    assert (cache_get (C, "www.google.com", "/", 80, dummy) != NULL);
    assert (cache_get (C, "www.youtube.com", "/", 80, dummy) != NULL);
    stall(9999);

    // Insert youtube2.com
    assert (cache_get (C, "www.youtube2.com", "/", 80, dummy) == NULL);
    cache_insert (C, "www.youtube2.com", "/", 80, large_string, BIG);
    assert (cache_get (C, "www.youtube.com", "/", 80, dummy) != NULL);
    assert (cache_get (C, "www.youtube2.com", "/", 80, dummy) != NULL);
    assert (cache_get (C, "www.google.com", "/", 80, dummy) == NULL);
    stall (9999);

    // Access youtube again
    assert (cache_get (C, "www.youtube.com", "/", 80, dummy) != NULL);

    // Check for evictions
    cache_insert (C, "www.google.com", "/", 80, large_string, BIG);
    assert (cache_get (C, "www.google.com", "/", 80, dummy) != NULL);
    assert (cache_get (C, "www.youtube.com", "/", 80, dummy) != NULL);
    assert (cache_get (C, "www.youtube2.com", "/", 80, dummy) == NULL);

    // Free our local variables
    free (dummy);
    cache_free (C);
    return 0;
}
