/*
 * mdb-lookup-server.c
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "mylist.h"
#include "mdb.h"

#define KeyMax 5


//loadmdb() and freemdb() were copied over from the lab4 solutions
int loadmdb(FILE *fp, struct List *dest)
{
    /*
     * read all records into memory
     */

    struct MdbRec r;
    struct Node *node = NULL;
    int count = 0;

    while (fread(&r, sizeof(r), 1, fp) == 1) {

        // allocate memory for a new record and copy into it the one
        // that was just read from the database.
        struct MdbRec *rec = (struct MdbRec *)malloc(sizeof(r));
        if (!rec)
            return -1;
        memcpy(rec, &r, sizeof(r));

        // add the record to the linked list.
        node = addAfter(dest, node, rec);
        if (node == NULL)
            return -1;

        count++;
    }

    // see if fread() produced error
    if (ferror(fp))
        return -1;

    return count;
}

void freemdb(struct List *list)
{
    // free all the records
    traverseList(list, &free);
    removeAllNodes(list);
}
static void die(const char *s) 
{ 
	perror(s); exit(1); 
}

int main(int argc, char **argv)
{

    // ignore SIGPIPE so that we don't terminate when we call
    // send() on a disconnected socket.
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
	die("signal() failed");
    
    if (argc != 3) {
        fprintf(stderr, "usage: %s <database-filename> <server-port>\n", argv[0]);
        exit(1);
    }

    unsigned short port = atoi(argv[2]);

    // Create a listening socket (also called server socket) 

    int servsock;
    if ((servsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        die("socket failed");

    // Construct local address structure

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // any network interface
    servaddr.sin_port = htons(port);

    // Bind to the local address

    if (bind(servsock, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
        die("bind failed");

    // Start listening for incoming connections

    if (listen(servsock, 5 /* queue size for connection requests */ ) < 0)
        die("listen failed");


    int clntsock;
    socklen_t clntlen;
    struct sockaddr_in clntaddr;

    while (1) {

        // Accept an incoming connection

        clntlen = sizeof(clntaddr); // initialize the in-out parameter

        if ((clntsock = accept(servsock,(struct sockaddr *) &clntaddr, &clntlen)) < 0)
            die("accept failed");
        // accept() returned a connected socket (also called client socket)
        // and filled in the client's address into clntaddr

	fprintf(stderr, "connection started from: %s\n", inet_ntoa(clntaddr.sin_addr));

    	char buf[10000];
	char toSend[10000];

	//stores the filename from the commandline argument
	const char *filename = argv[1];
    	FILE *fp = fopen(filename, "rb");
    	if (fp == NULL)
        	die(filename);

	//creates and intializes list
    	struct List list;
    	initList(&list);
	
	//loads records from database into the list
    	int loaded = loadmdb(fp, &list);
    	if (loaded < 0)
        	die("loadmdb");

    	fclose(fp);

	FILE *input = fdopen(clntsock , "r");
	char key[KeyMax + 1];

        while (fgets(buf, sizeof(buf), input)) 
	{
                // must null-terminate the string manually after strncpy().
        	strncpy(key, buf, sizeof(key) - 1);
        	key[sizeof(key) - 1] = '\0';

        	// if newline is there, remove it.
        	size_t last = strlen(key) - 1;
        	if (key[last] == '\n')
            		key[last] = '\0';

        	// traverse the list, printing out the matching records
        	struct Node *node = list.head;
        	int recNo = 1;
        	while (node) 	
		{
            		struct MdbRec *rec = (struct MdbRec *)node->data;
            		if (strstr(rec->name, key) || strstr(rec->msg, key)) 
			{
                		int x = snprintf(toSend, 100, "%4d: {%s} said {%s}\n", recNo, rec->name, rec->msg);

				if(send(clntsock, toSend, x, 0) != x)
				{
					fprintf(stderr, "send failed\n");
       					break;
				}

            		}
            		node = node->next;
            		recNo++;
        	}

        	//we no longer send a lookup prompt to the user 
		//printf("\nlookup: ");
        	//fflush(stdout);
		
		int y = snprintf(toSend, 100, "\n");
		if(send(clntsock, toSend, y, 0) != y)
		{
			fprintf(stderr, "send failed\n");
			break;
		}

        }

	fprintf(stderr, "connection terminated from: %s\n", inet_ntoa(clntaddr.sin_addr));
       
	// close the client socket wrapped in the file and go back to accept()	
	fclose(input);
    }
}
