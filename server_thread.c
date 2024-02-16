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

	struct cache_buf * cache;

	struct requestBuf * requests;
};
	
	pthread_mutex_t lru_lock;
	pthread_mutex_t cache_lock;
	struct lru * lru_head; 
	struct lru * lru_tail;

struct lru{
	
	char *file_name;
	struct lru *prev;
	struct lru *next;
	
};

struct cache_entry {

	struct file_data * file;
	int inUse;
	struct cache_entry * next;

};

struct cache_buf{

	int size;
	int total_size;
	struct cache_entry **table;
};

void lru_insert(struct lru * node){

	if(lru_head == NULL){
		lru_head = node;
		lru_tail = node;
	} else if (lru_head != NULL){
		node->next = lru_head;
		lru_head = node;
	}
	
}

struct lru *lru_find(char * key){
	
	struct lru * node;
	node = lru_head;

	while(node != NULL){
	
		if(strcmp(node->file_name, key) == 0)
			return node;
		node = node->next;
	}
	return NULL;
}

struct lru * lru_remove(struct lru * node){
	
	if(node == NULL){

		if(node->prev == NULL){
			lru_head = node->next;
		} else if(node->prev != NULL){
			node->prev->next = node->next;
		}
		
		if(node->next == NULL){
			lru_tail = node->prev;
		} else {
			node->next->prev = node->prev;
		}
		
		return node;
	}
	return NULL;
}

void clear_lru(){
	
	struct lru * node = lru_head;
	struct lru * nNode = NULL;

	while(node != NULL){

		nNode = node->next;
		free(node->file_name);
		free(node);
		node = nNode;
	}
}
/*
struct lru * pop_lru(char * key){
	
}
*/

int hash (char * str, int size){
	int hash = 2*strlen(str) + 1;
	int c = 0;

	while(c < strlen(str)){
		hash = hash * 33 + str[c];
		c++;
	}
	
	return abs(hash % (size));
}

struct cache_buf * cache_init(int max_cache_size){
	
	struct cache_buf * cache = (struct cache_buf*)malloc(sizeof(struct cache_buf));
	cache->total_size = 0;	
	cache->size = 2000000;
	lru_head = NULL;
	lru_tail = NULL;
	pthread_mutex_init(&cache_lock, NULL);
	
	return cache;	
}

static struct file_data * file_data_init(void);
static void file_data_free(struct file_data *data);
int cache_evict(struct cache_buf * cache, int evictSize);


struct cache_entry * cache_lookup(struct cache_buf * cache, char * key){

	int i = hash(key, cache->size);
	struct cache_entry * node = cache->table[i];
	
	while(node != NULL){
		
		if(strcmp(node->file->file_name, key) == 0){
			return node;
		}
		node = node->next;
	}	
	return NULL;

}


struct cache_entry * cache_insert(void * server, struct file_data * data){

	char * key = data->file_name;
	int size = data->file_size;
	int i, evictSize, evictNode;
	struct server * sv = (struct server *) server;
	
	if(size > sv->max_cache_size){
		return NULL;
	}
	
	struct cache_entry * node = cache_lookup(sv->cache, key);

	if(node != NULL){
		return NULL;
	}
	
	evictSize = sv->cache->total_size + size - sv->max_cache_size;
	evictNode = cache_evict(sv->cache, evictSize);
	
	if(evictNode == 0){
		return NULL;
	}

	i = hash(key, sv->cache->size);
	struct cache_entry * new = (struct cache_entry*)malloc(sizeof(struct cache_entry));

	new->file = file_data_init();
	new->file->file_name = strdup(key);
	new->file->file_buf = strdup(data->file_buf);
	new->file->file_size = size;
	new->next = sv->cache->table[i];
	new->inUse = 0;
	sv->cache->table[i] = new;
	sv->cache->total_size += size;
	
	
	struct lru * lNode = (struct lru *)malloc(sizeof(struct lru));
	lNode->file_name = strdup(key);
	lNode->next = NULL;
	lNode->prev = NULL;

	lru_insert(lNode);
	return new;
}

int cache_evict(struct cache_buf * cache, int evictSize){

	int	oldSize = evictSize;
	if(evictSize <= 0)
		return 1;

	struct lru * node = lru_tail;
	struct cache_entry * prevNode = NULL;
	struct cache_entry * curNode = NULL;
	struct cache_entry * deleteNode;
	char * key;
	int i;
 
	while(evictSize > 0 && node != NULL){
		
		key = lru_tail->file_name;
		deleteNode = cache_lookup(cache, key);
		evictSize -=deleteNode->file->file_size;
		node = node->prev;
	}
	evictSize = oldSize;

	while(evictSize > 0 && lru_tail != NULL){
		
		key = lru_tail->file_name;
		deleteNode = cache_lookup(cache, key);
		
		if(deleteNode->inUse) break;

		i = hash(key, cache->size);
		curNode = cache->table[i];
		
		while(curNode != deleteNode){
			prevNode = curNode;
			curNode = curNode->next;
		}
		
		if(prevNode != NULL){
			prevNode->next = deleteNode->next;
		} else {
			cache->table[i] = deleteNode->next;
		}
		
		cache->total_size -= deleteNode->file->file_size;

		file_data_free(deleteNode->file);
		free(deleteNode);
	}

	return 1;
}

void cache_destroy (struct cache_buf * cache){
}
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
	struct cache_entry * node;
	struct lru * lruNode;
	
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
	
	if(sv->max_cache_size == 0){

		ret = request_readfile(rq);
		
		if (ret == 0) { /* couldn't read file */
				
			goto out;
		}

		request_sendfile(rq);
	}

	pthread_mutex_lock(&cache_lock);
	node = cache_lookup(sv->cache,data->file_name);

	if(node != NULL){
		
		node->inUse = 1;
		data->file_size = node->file->file_size;
		data->file_buf = strdup(node->file->file_buf);
		request_set_data(rq, data);
		lruNode = lru_find(data->file_name);
		lru_remove(lruNode);
		lru_insert(lruNode);

	} else {
	
		pthread_mutex_unlock(&cache_lock);
		ret = request_readfile(rq);
			
		if (ret == 0) { /* couldn't read file */
			goto out;
		}
	
		pthread_mutex_lock(&cache_lock);
		node = cache_insert(sv, data);

		if(node != NULL){
		
			lruNode = lru_find(data->file_name);
			lru_remove(lruNode);
			lru_insert(lruNode);

		}

		pthread_mutex_unlock(&cache_lock);
		request_sendfile(rq);
	}

	
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
			sv->requests  = malloc(sizeof (struct requestBuf));

			sv->requests->size = max_requests + 1;
			sv->requests->in = 0;
			sv->requests->out = 0;

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

	//printf("\nInitialization Complete\n");
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
