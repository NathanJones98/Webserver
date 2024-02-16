#include "request.h"
#include "server_thread.h"
#include "common.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>



struct requestBuf{
	int in;
	int out;
	int size;
	int * array;
};

struct server {
	int nr_threads;
	int max_requests;
	int max_cache_size;
	int exiting;

	/* add any other parameters you need */
	pthread_t *threads;
	pthread_mutex_t lock;
	pthread_cond_t full;
	pthread_cond_t empty;

	struct requestBuf * requests;
};

/* static functions */

/* initialize file data */
static struct file_data *
file_data_init(void)
{
	struct file_data *data;

	data = Malloc(sizeof(struct file_data));
	data->file_name = NULL;
	data->file_buf = NULL;
	data->file_size = 0;
	return data;
}

/* free all file data */
static void
file_data_free(struct file_data *data)
{
	free(data->file_name);
	free(data->file_buf);
	free(data);
}

static void
do_server_request(struct server *sv, int connfd)
{	
	
	int ret;
	struct request *rq;
	struct file_data *data;

	data = file_data_init();

	/* fill data->file_name with name of the file being requested */
	rq = request_init(connfd, data);

	if (!rq) {
		file_data_free(data);
		return;
	}

	/* read file, 
	 * fills data->file_buf with the file contents,
	 * data->file_size with file size. */
	
	ret = request_readfile(rq);


	if (ret == 0) { /* couldn't read file */
		
		goto out;
	}

	/* send file to client */
	request_sendfile(rq);
out:
	request_destroy(rq);
	file_data_free(data);
}

void* thread_loop(void * server){

	int temp;
	//printf("\nThread Loop Running\n");
	struct server * sv = (struct server *) server;

	while(1){
		
		printf("\nTWO\n");
		pthread_mutex_lock(&sv->lock);
	
		while (sv->requests->out == sv->requests->in && sv->exiting == 0){
			printf("Thread Waiting2\n");
			pthread_cond_wait(&sv->empty, &sv->lock);
			printf("Thread Waking\n");
		}
		
		printf("\nOne\n");

		if(sv->exiting != 0){
			pthread_mutex_unlock(&sv->lock);
			pthread_exit(NULL);
		}

		temp = sv->requests->array[sv->requests->out];

		sv->requests->out = (sv->requests->out + 1) % sv->requests->size;
		
		pthread_cond_signal(&sv->full); 	

		pthread_mutex_unlock(&sv->lock);

		//pthread_cond_signal(&sv->full);
		
		printf("Doing Server Request");
		do_server_request(sv, temp);
	}	
}

/* entry point functions */

struct server *
server_init(int nr_threads, int max_requests, int max_cache_size)
{	
	//printf("\nInitialization Starting\n");
	struct server *sv;
	int i = 0;

	sv = Malloc(sizeof(struct server));
	sv->nr_threads = nr_threads;
	sv->max_requests = max_requests+1;
	sv->max_cache_size = max_cache_size;
	sv->exiting = 0;
	
    sv->requests  = malloc(sizeof (struct requestBuf));

	sv->requests->size = max_requests + 1;
	sv->requests->in = 0;
	sv->requests->out = 0;

	pthread_mutex_init(&sv->lock, NULL);
	pthread_cond_init(&sv->empty, NULL);
	pthread_cond_init(&sv->full, NULL);

	if(nr_threads > 0 || max_requests > 0 || max_cache_size > 0){

	/* Lab 4: create queue of max_request size when max_requests > 0 */

		if(max_requests > 0)
			sv->requests->array = malloc((sv->requests->size)*sizeof(int));

	
	/* Lab 5: init server cache and limit its size to max_cache_size */
	
	
	/* Lab 4: create worker threads when nr_threads > 0 */	

		if(nr_threads > 0)
		{
			sv->threads = malloc(sizeof(pthread_t)*nr_threads);	
		} else {
			sv->threads = NULL;
		}

		while(i < nr_threads)
		{
			pthread_create(&sv->threads[i], NULL, thread_loop, sv);
			i++;
		}
	}

	return sv;

	printf("\nInitialization Complete\n");
}

void
server_request(struct server *sv, int connfd)
{	
	
	//printf("\nserver_request Running\n");

	if (sv->nr_threads == 0) { /* no worker threads */
		do_server_request(sv, connfd);
	} else {

		pthread_mutex_lock(&sv->lock);
		
		while( sv->requests->in == (sv->requests->out+1)%sv->requests->size){
			pthread_cond_wait(&sv->full, &sv->lock);	
		}

		sv->requests->array[sv->requests->in] = connfd;

		sv->requests->in = (sv->requests->in +1) % sv->requests->size;

		pthread_mutex_unlock(&sv->lock);

		pthread_cond_signal(&sv->empty);

		/*  Save the relevant info in a buffer and have one of the
		 *  worker threads do the work. */		
	}
}

void
server_exit(struct server *sv)
{

	int i = 0;
	/* when using one or more worker threads, use sv->exiting to indicate to
	 * these threads that the server is exiting. make sure to call
	 * pthread_join in this function so that the main server thread waits
	 * for all the worker threads to exit before exiting. */
	sv->exiting = 1;
	
	pthread_cond_broadcast(&sv->empty);

	for(i = 0; i  < sv->nr_threads; i++){

		pthread_t thread = sv->threads[i];
		pthread_join(thread, NULL);
	}
	
	free(sv->threads);
	free(sv->requests->array);
	free(sv->requests);
	//free(sv->lock);
	//free(sv->full);
	//free(sv->empty);
	/* make sure to free any allocated resources */
	free(sv);
}
