#ifndef WEB_DATA_H
#define WEB_DATA_H

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "web_data.h"

struct web_data_hdr
{
	char *website;
	char *file;
	int port;
	char *data;
	int data_size;
	clock_t acc_time;
};
typedef struct web_data_hdr *web_data;

web_data web_data_new (char *website, char *file, int port, 
    char *data, int dataSize);
void web_data_free (web_data w);
int web_data_equals (web_data w, char *website, char *file, int port);
int web_data_size (web_data w);
void web_data_update_acc_time (web_data w);

#endif
