/**

Author: Ananya Kumar
Organization: Carnegie Mellon University
Description: Cache Simulator
Notes: We assume that the cache size does not exceed 2^30 bytes ~ 1GB

**/

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "contracts.h"
#include "cachelab.h"

/* Explains to the user how they should use the function */
void printUsage ()
{
	fprintf(stderr, "Usage: ./csim  [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n");
	fprintf(stderr, "Options:\n"
 				 	  "  -h         Print this help message.\n"
					  "  -v         Optional verbose flag.\n"
					  "  -s <num>   Number of set index bits.\n"
					  "  -E <num>   Number of lines per set.\n"
					  "  -b <num>   Number of block offset bits.\n"
					  "  -t <file>  Trace file.\n\n"

					"Examples:\n"
					  "  linux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace\n"
					  "  linux>  ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}

/* malloc but exits on failure */
void *xmalloc (size_t len)
{
	void *ptr = malloc(len);

	if (ptr == NULL) {
		fprintf(stderr, "Could not allocate memory.\n");
		exit(1);
	} 

	else return ptr;
}

/* calloc but exits on failure */
void *xcalloc (size_t num, size_t size)
{
	void *ptr = calloc(num, size);

	if (ptr == NULL) {
		fprintf(stderr, "Could not allocate memory.\n");
		exit(1);
	} 

	else return ptr;
}

/** Returns unsigned 64-bit int with lowest i bits set to 1 */
unsigned long long int mask (int i)
{
	REQUIRES(0 <= i && i <= 64);
	unsigned long long int m = 1;
	return (m<<i)-1;
}

int main (int argc, char* argv[])
{
	int opt;
	int s = -1, E = -1, b = -1;
	char *fileName = NULL;
	bool verbose = false;
	int i; //Loop variables

	/** Parse command line arguments **/

	while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1)
	{
		switch (opt) 
		{
			case 'v':
				verbose = true;
				break;
			case 's':
				s = atoi(optarg);
				break;
			case 'E':
				E = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
				break;
			case 't':
				fileName = xmalloc((strlen(optarg)+1) * sizeof(char));
        strcpy(fileName, optarg);
				break;
			case 'h':
			default:
				printUsage();
				exit(0);
		}
	}

	if (s < 1 || E < 1 || b < 1 || s > 30 || E > 30 || !fileName) {
		printUsage();
		exit(0);
	}

	FILE *tracePtr = fopen(fileName, "r");

	if (!tracePtr) {
		printUsage();
		exit(0);
	}

	/** Set up cache data structures **/

	int sets = 1 << s; //Assume we won't have >= 2^31 sets

	unsigned long long int **cacheTag = xmalloc(sets * 
                                              sizeof(unsigned long long int *));
	//cacheTag[i][j] stores the tag in the j^th line of the i^th set

	unsigned long long int **timeStamp = xmalloc(sets * 
                                              sizeof(unsigned long long int *));
	//timeStamp[i][j] stores the time when the j^th line was added to the i^th set

	for (i = 0; i < sets; i++)
	{
		cacheTag[i] = xmalloc(E * sizeof(unsigned long long int));
		timeStamp[i] = xcalloc(E, sizeof(unsigned long long int));
	}

	/** Simulate Cache **/

	char line[50];
	int curTime = 1;
	int misses = 0, hits = 0, evictions = 0;

	while (fgets(line, 50, tracePtr) != NULL) 
	{
		unsigned long long int address;
		int size;
		char op;

		sscanf(line, " %c %llx,%d", &op, &address, &size);
		if (verbose) printf("%c %llx,%d ", op, address, size);

		if (op == 'I') continue; //Ignore instruction operations

		unsigned long long int setIndex = (address>>b) & mask(s); //Extract set
		unsigned long long int *curSetTag = cacheTag[setIndex];
		unsigned long long int *curSetStamp = timeStamp[setIndex];

		unsigned long long int tag = (address>>(b+s)) & mask(64-b-s); //Extract tag

		//Search lines of appropriate set to check if address is in cache

		int tagIdx = -1; //Which line of cache is the current address in?

		for (i = 0; i < E; i++) 
		{
			if (curSetStamp[i] != 0 && curSetTag[i] == tag) {
				tagIdx = i;
				break;
			}
		}

		//If the memory block is in cache, we have a hit!

		if (tagIdx != -1) {
			curSetStamp[tagIdx] = curTime; //Update time for LRU algorithm
			hits++;
			if (verbose) printf("Hit ");
		}

		//Otherwise, we need to store memory block in cache

		else {

			misses++;
			if (verbose) printf("Miss ");

			//Find LRU

			int lruIdx = 0; //Address of least recently used line

			for (i = 0; i < E; i++)
			{
				if (curSetStamp[i] <= curSetStamp[lruIdx]) {
					lruIdx = i;
				}
			}

			if (curSetStamp[lruIdx]) {
				evictions++;
				if (verbose) printf("Evicted ");
			} 

			curSetTag[lruIdx] = tag;
			curSetStamp[lruIdx] = curTime;
		}

		//The 2nd step of a modify operation is always a hit

		if (op == 'M') {
			hits++;
			if (verbose) printf("Hit ");
		}

		curTime++;
		if (verbose) printf("\n");
	}

  //Free allocated data
  
  for (i = 0; i < sets; i++)
  {
    free(cacheTag[i]);
    free(timeStamp[i]);
  }

  free(cacheTag);
  free(timeStamp);
  free(fileName);
  fclose(tracePtr);

	//print final statistics
	printSummary(hits, misses, evictions);
	return 0;
}
