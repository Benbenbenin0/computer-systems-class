#include "web_data.h"

web_data web_data_new (char *website, char *file, int port, 
    char *data, int dataSize) 
{
    web_data w = malloc(sizeof(struct web_data_hdr));
	  w->website = malloc(strlen(website)+1);
	  w->file = malloc(strlen(file)+1);
	  w->data = malloc(dataSize);
	  
    strcpy (w->website, website);
	  strcpy (w->file, file);
    memcpy (w->data, data, dataSize);
	  w->port = port;
	  w->data_size = dataSize;
    web_data_update_acc_time (w);
    
    return w;
}

// Free the pointer of type web_data
void web_data_free (web_data w) 
{
    free (w->website);
    free (w->file);
    free (w->data);
    free (w);
}

int web_data_equals (web_data w, char *website, char *file, int port) 
{
    if (website == NULL)    return 0;
    if (file == NULL)   return 0;
	  return (strcmp (w->website, website) == 0
		  &&	strcmp (w->file, file) == 0
		  &&	w->port == port);
}

void web_data_update_acc_time (web_data w) 
{
    // clock is thread safe according to
    // http://www.keil.com/support/man/docs/armlibref/armlibref_Chdiedfe.htm
    w->acc_time = clock ();
}
