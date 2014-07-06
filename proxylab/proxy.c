/*
Authors: Ananya Kumar, Ray Li
Description: A simple HTTP proxy that supports GET requests
and caches web data
*/

#include <stdio.h>
#include <assert.h>
#include "csapp.h"
#include "cache.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* Core functions */
void process(int fd);
void *thread (void *vargp);

/* Network communication functions */
int send_request(int fd, char *dir);
int send_proxyheaders(int webfd, int hostSpecified, const char *name);
int client_to_web(rio_t rio, int webfd, int *hostSpecified);
int web_to_client(int webfd, int fd, char *name, char *dir, int port);
int cache_to_client(const char* data, int dataSize, int fd);

/* Signal Handling */
void sigint_handler(int sig);

/* Cache Functions */
const char *retrieve_cache(char *name, char *dir, int port, int *dataSize); 

/* String Parsing Functions */
char *get_website(char *uri);
void get_uri_info(char *uri, char *name, char *dir, int *port);
void strip_space (char *s);
int in_list (const char *s, const char **slist, int listSize);

/* Utilities */
int min (int x, int y);
void clienterror(int fd, char *cause, char *errnum, 
         char *shortmsg, char *longmsg);

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";
static const char *connect_hdr = "Connection: close\r\n";
static const char *proxy_connect_hdr = "Proxy-Connection: close\r\n";
static const char *change_headers[5] = {"User-Agent", "Accept", 
    "Accept-Encoding", "Connection", "Proxy-Connection"};

const int verbose = 0;

/* The cache stores recently accessed web content for fast retrieval */
cache webStore;
sem_t read_m, write_m;
int read_cnt;

int main(int argc, char **argv)
{
    int listenfd, port, clientlen;
    int *connfdp;
    pthread_t tid;
    struct sockaddr_in clientaddr;

    /* Install custom signal handlers */

    Signal(SIGINT,  sigint_handler);
    Signal(SIGPIPE, SIG_IGN);

    /* Set up cache and semaphores for threading */
    webStore = cache_new();
    sem_init(&read_m, 0, 1);
    sem_init(&write_m, 0, 1);
    read_cnt = 0;

    /* Check command line args */
    if (argc != 2) 
    {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
    }

    port = atoi(argv[1]);

    /* Listen for client connections */

    listenfd = Open_listenfd(port);
    clientlen = sizeof(clientaddr);

    while (1) 
    {
		if ((connfdp = malloc(sizeof(int))) == NULL)
		{
			fprintf(stderr, "Memory allocation error\n");
		}

		else
		{
			if ((*connfdp = Accept(listenfd, (SA *)&clientaddr, 
				  (socklen_t *)&clientlen)) < 0)
			{
				fprintf(stderr, "Could not accept client connection.\n");
			}

			else
			{
				pthread_create(&tid, NULL, thread, (void *)connfdp);
			}
		}
    }

    return 0;
}

/*****************
 * Core Functions
 *****************/

/*  New threads that handle client requests should start here.
    vargp should point to an integer containing the file descriptor
    for the new network connection. */
void *thread (void *vargp)
{
	pthread_detach(pthread_self());
	int connfd = *((int *)vargp);
	free(vargp);
	process(connfd);
	close(connfd);
	return NULL;
}

/*  Core proxy function. Retrieves HTTP request from client and
    serves webpage (from cache if it already exists, and from
    the server otherwise).
    fd should be the file descriptor of a socket connected to
    the client. */
void process(int fd)
{
   	char buf[MAXLINE];
    rio_t rio;
    rio_readinitb(&rio, fd);

    /* Read Request Line */

    if (rio_readlineb(&rio, buf, MAXLINE) <= 2)
	{
        clienterror(fd, "GET", "400", "Bad Request",
                "Invalid syntax: every line must end with \\r\\n");
        fprintf(stderr, "Could not read client request line\n");
		return;
	}

	/* Extract information from header */

	char method[MAXLINE], uri[MAXLINE], version[MAXLINE];

	if (sscanf(buf, "%s %s %s", method, uri, version) < 3)
	{
		clienterror(fd, method, "400", "Bad Request",
                "Invalid syntax for GET request");
        fprintf(stderr, "Invalid header format %s\n", buf);
		return;
	}

	if (strcasecmp(method, "GET"))
	{ 
        clienterror(fd, method, "501", "Not Implemented",
                "Proxy only supports the GET method");
        fprintf(stderr, "Invalid header format: %s\n", buf);
        return;
    }

    /* Get hostname and port from URI */

    char name[MAXLINE], dir[MAXLINE];
    int port;
    get_uri_info(uri, name, dir, &port);

    if (verbose)
    	printf("Request: %s %s %d\n", name, dir, port);

    /* Check cache for desried content */

    int dataSize;
    const char *data = retrieve_cache(name, dir, port, &dataSize);
    
    if (data != NULL)
    {
        /* Read rest of HTTP request from client */
        while (rio_readlineb(&rio, buf, MAXLINE) > 2) {}

        if (cache_to_client(data, dataSize, fd) == -1)
            fprintf(stderr, "Error sending data from cache to client\n");

        return;
    }

    /* Connect to website server */

    int webfd = open_clientfd_r(name, port);

    if (webfd < 0)
    {
        clienterror(fd, method, "502", "Bad Gateway",
                "Proxy could not connect to web server");
        fprintf(stderr, "Error connecting to web server\n");
        return;
    }

    /* Send http request line */

    if (send_request(webfd, dir) == -1)
    {
        clienterror(fd, method, "502", "Bad Gateway",
                "Proxy could not send HTTP request to web server.");
        fprintf(stderr, "Error writing request to web server\n");
        return;
    }

    /* Read from client and send http to web server */

    int hostSpecified = 0;

    if (client_to_web(rio, webfd, &hostSpecified) == -1)
    {
        clienterror(fd, method, "502", "Bad Gateway",
                "Proxy could not write data to web server");
        fprintf(stderr, "Error forwarding client HTTP to web\n");
        return;
    }

    /* Write headers to web server */

    if (send_proxyheaders(webfd, hostSpecified, name) == -1) //Other headers
    {
        clienterror(fd, method, "502", "Bad Gateway",
                "Proxy could not write header data to web server");
        fprintf(stderr, "Error sending proxy headers to server\n");
        return;
    }

	//Send terminating line to web server
	rio_writen(webfd, "\r\n", strlen("\r\n"));	

    /* Forward server data to client */

    if (verbose)
    	printf("Awaiting website response\n");

    if (web_to_client(webfd, fd, name, dir, port) == -1)
    {
        clienterror(fd, method, "502", "Bad Gateway",
                "Proxy could not read web data from web server");
        fprintf(stderr, "Error forwarding web data to client\n");
        return;
    }

    if (verbose)
    	printf("Served webpage\n");

    close(webfd);
}

/*******************
 * Web Communication
 *******************/

/*  Send the HTTP request line to the web server.
    fd is a file descriptor to the web server.
    dir is the directory of the file the client wants */
int send_request(int fd, char *dir)
{
    char request[MAXLINE] = "GET /";
    strcat(request, dir);
    strcat(request, " HTTP/1.0\r\n");
    int reqLen = strlen(request);

    if (reqLen != rio_writen(fd, request, reqLen))
        return -1;

    if (verbose)
        printf("New Request:\n%s", request);

    return 0;
}

/*  Send the proxy headers to the web server
    webfd is a file descriptor to the web server
    hostSpecified is true iff the client has already specified a
        host (in which case we simply use the host the client specified)
    name is the name of the web server */
int send_proxyheaders(int webfd, int hostSpecified, const char *name)
{

/* Error handled macro that saves a lot of repetitive code.
Sends msg to fd, returns -1 on error. */
#define RIO_WRITEN(FD, MSG) \
{ \
    strcpy(buf, MSG); \
    int msgsize = strlen(buf); \
    if (rio_writen(FD, buf, msgsize) != msgsize) \
        return -1; \
    if (verbose) \
        printf("%s", buf); \
}

    char buf[MAXLINE];
    
    if (!hostSpecified) //Host header
    {
        strcpy(buf, "Host: ");
        strcat(buf, name);
        strcat(buf, "\r\n");
        int msgsize = strlen(buf);
        
        if (rio_writen(webfd, buf, msgsize) != msgsize)
            return -1;
        if (verbose)
            printf("%s", buf);
    }

    RIO_WRITEN(webfd, user_agent_hdr);
    RIO_WRITEN(webfd, accept_hdr);
    RIO_WRITEN(webfd, accept_encoding_hdr);
    RIO_WRITEN(webfd, connect_hdr);
    RIO_WRITEN(webfd, proxy_connect_hdr);
    return 0;

#undef RIO_WRITEN
}

/*  forwards data from client to web server, hostSpecified
    is set to 1 if the client specifies the host and 0 otherwise.
    Ignores headers defined in change_headers
    rio is RIO state for the client.
    webfd is a file descriptor for the webserver. */
int client_to_web(rio_t rio, int webfd, int *hostSpecified)
{
    char* headIdx;
    char header[MAXLINE];
    char buf[MAXLINE];
    int len;
    *hostSpecified = 0;

    while ((len = rio_readlineb(&rio, buf, MAXLINE)) != 2)
    {
        if (len < 2)
            return -1;

        //Process header

        headIdx = strchr(buf, ':');

        if (headIdx)
        {
            //Extract the header
            size_t headSize = (size_t)(headIdx-buf);
            strncpy(header, buf, headSize);
            header[headSize] = '\0';
            strip_space(header);

            if (!strcasecmp(header, "Host"))
                *hostSpecified = 1;

            if (in_list(header, change_headers, 5))
                continue;
        }

        if (verbose)
            printf("%s", buf);

        //Forward line to web server

        if (len != rio_writen(webfd, buf, len))
            return -1;
    }

    return 0;
}

/*  Forwards web page from web server to client.
    Caches web data if it fits within MAX_OBJECT_SIZE
    webfd is file descriptor for web server.
    fd is file descriptor for client.
    name, dir, port are the server's name, directory, port */
int web_to_client(int webfd, int fd, char *name, char *dir, int port)
{
    rio_t rioWeb;
    rio_readinitb(&rioWeb, webfd);
    char buf[MAXLINE];
    int len;

    char *cacheBuf = malloc(MAX_OBJECT_SIZE);
    char *cacheBufHead = cacheBuf;
    int cacheBufSize = 0;

    while ((len = rio_readnb(&rioWeb, buf, MAXLINE)) != 0)
    {
        if (len < 0)
        {
            free(cacheBuf);
            return -1;
        }

        if (len != rio_writen(fd, buf, len))
        {
            free(cacheBuf);
            return -1;
        }

        if (cacheBufSize != -1)
        {
            cacheBufSize += len;

            if (cacheBufSize <= MAX_OBJECT_SIZE)
            {
                memcpy(cacheBufHead, buf, len);
                cacheBufHead += len;
            }

            else
                cacheBufSize = -1;
        }
    }


    if (cacheBufSize != -1)
    {
        P(&write_m);
        cache_insert(webStore, name, dir, port, cacheBuf, cacheBufSize);
        V(&write_m);
    }

    free(cacheBuf);
    return 0;
}

/*  Sends dataSize bytes from data to the client, specified by
    file descriptor fd */
int cache_to_client(const char* data, int dataSize, int fd)
{
    int dataTransfer = 0;
    int len;

    while (dataTransfer < dataSize)
    {
        len = min(MAXLINE, dataSize-dataTransfer);

        if (len != rio_writen(fd, (void *)data, len))
            return -1;

        data += len;
        dataTransfer += len;
    }

    assert(dataTransfer == dataSize);
    return 0;
}

/*****************
 * Signal Handling
 *****************/

/* Custom sig-int handler to free cache memory and gracefully exit */
void sigint_handler(int sig)
{
    cache_free(webStore);
    printf("Thank you for using AR proxy!\n");
    exit(1);
}

/*****************
 * Cache Functions
 *****************/

/*  Thread safe function that gets web data specified by
    name, dir, port of web server. Returns NULL if no corresponding
    entry exists in the cache. Otherwise, returns pointer to the data
    (which should not be modified), and stores size of data in 
    *dataSize */
const char *retrieve_cache(char *name, char *dir, int port, int *dataSize)
{
    P(&read_m);
    if (read_cnt == 0)
        P(&write_m);
    read_cnt++;
    V(&read_m);

    const char *data = cache_get(webStore, name, dir, port, dataSize);

    P(&read_m);
    read_cnt--;
    if (read_cnt == 0)
        V(&write_m);
    V(&read_m);

    return data;
}

/*****************
 * String Parsing
 *****************/

/*  Ignores "http://" and the like in the uri
    For example if uri is "http://www.google.com" the
    pointer returned points to "www.google.com" */
char *get_website (char *uri)
{
    char *website;

    if ((website = strstr(uri, "://")))
        return website+3;
    else
        return uri;
}

/*  Extracts the name of the website, directory, and port from a URI
    If dir is not specified modifies website to make the directory "/".
    If port is not specified assume it is 80 */
void get_uri_info(char *uri, char *name, char *dir, int *port)
{
    //Ignore the "http://" part of the website

    char *website = get_website(uri);

    //Find port no. and directory by searching for ':' and "/"

    char *portIdx = strchr(website, ':');
    char *dirIdx = strchr(website, '/');

    if (!dirIdx)
    {
        strcat(website, "/");
        dirIdx = strchr(website, '/');
        assert(website+(strlen(website)-1) == dirIdx);
    }

    //Is the port number specified?

    int portBefDir = (int)(dirIdx-portIdx);

    if (portIdx && portBefDir > 0)
    {
        *port = atoi(portIdx+1);
        int nameSize = (size_t)(portIdx-website);
        strncpy(name, website, nameSize);
        name[nameSize] = '\0';
    }

    else
    {
        *port = 80;
        int nameSize = (size_t)(dirIdx-website);
        strncpy(name, website, nameSize);
        name[nameSize] = '\0';
    }

    strcpy(dir, dirIdx+1);
}

/*  Checks if a string s is in slist, a char array contain listSize
    elements */
int in_list (const char *s, const char **slist, int listSize)
{
    int i;

    for (i = 0; i < listSize; i++)
    {
        if (!strcasecmp(s, slist[i]))
            return 1;
    }

    return 0;
}

/* Removes spaces from the given string */
void strip_space (char *s)
{
	int i = 0;
	int sp_loc = 0;

	while (s[i] != '\0')
	{
		if (s[i] != ' ')
			sp_loc = i+1;
		i++;
	}

	s[sp_loc] = '\0';
}

/*****************
 * Utilities
 *****************/

/* Return the min of 2 numbers */
int min (int x, int y)
{
    if (x < y)
        return x;
    return y;
}

/* Returns an error message to the client */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    rio_writen(fd, buf, strlen(buf));
    rio_writen(fd, body, strlen(body));
}
/* $end clienterror */
