/* CSci4061 F2018 Assignment 3

Group number: 5
date: 12/10/2018
name: Lee Wintervold, Jack Bartels, Joe Keene
id: Winte413, Barte285, Keen0129 */


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include "util.h"
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>

#define INVALID -1
#define BUFF_SIZE 1024

/*
  THE CODE STRUCTURE GIVEN BELOW IS JUST A SUGESSTION. FEEL FREE TO MODIFY AS NEEDED
*/

// locks:
pthread_mutex_t q_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t c_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t l_lock = PTHREAD_MUTEX_INITIALIZER;

// CVs:
pthread_cond_t q_not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t q_not_full = PTHREAD_COND_INITIALIZER;

// structs:
typedef struct request_entry_t {
   int fd;
   char *request;
} request_entry_t;

typedef struct cache_entry_t {
   int content_size;
   char *request;
   char *content;
   struct cache_entry_t *next;
   struct cache_entry_t *prev;
} cache_entry_t;

typedef struct cache_info_t {
  int cache_size;
  struct cache_entry_t *front;
  struct cache_entry_t *rear;
} cache_info_t;

typedef struct cache_map_entry_t {
  struct cache_entry_t *cache_entry;
  struct cache_map_entry_t *collision;
} cache_map_entry_t;

// global objects

// PROTECT WITH q_lock
request_entry_t *request_q;
int q_in_idx = 0;
int q_out_idx = 0;
int q_count = 0;
int worker_count;

// PROTECT WITH c_lock
cache_map_entry_t **hashtable;
cache_info_t cache_info = {0};
int max_cache_size;

// PROTECT WITH l_lock
FILE *log_file;


int min_worker_count;
int max_q_size;
int max_hash_size;
int dynamic_thread_flag;

/* ************************ Dynamic Pool Code ***********************************/
// Extra Credit: This function implements the policy to change the worker thread pool dynamically
// depending on the number of requests
void * dynamic_pool_size_update(void *arg) {
  while(1) {
    // Run at regular intervals
    // Increase / decrease dynamically based on your policy
  }
}
/**********************************************************************************/

/* ************************************ Cache Code ********************************/

int is_prime(int x){
  // Slow but acceptable for small numbers, less than one million
  if (x == 2){
   return 1;
  }
  for(int n = 2; n < x; n++){
    if(x % n == 0){
      return 0;
    }
  }
  return 1; 
}

int get_prev_prime(int x){
  if (x == 0 || x == 1){
    return 2;
  }
  if (x % 2 == 0){
    x++;
  }
  while (1){
    if (is_prime(x)){
    return x;
    } else {
      x = x + 2;
    }
  }
}

int hash_request(char *request){
  // hash string, return index of hashtable
  // http://www.cse.yorku.ca/~oz/hash.html
  
  int hash = 5381;
  int c;
  while(c = *request++){
    hash = ((hash << 5) + hash) + c;
  }
  if (hash == INT_MAX){
    hash = 0;
  } else if (hash < 0){
    hash = -1 * hash;
  }

  return hash % max_hash_size;
}

// Function to check whether the given request is present in cache
cache_entry_t *getCacheEntry(char *request){
  // return pointer to entry if the request is present in the cache
  
  int idx = hash_request(request);
  cache_map_entry_t *traverse = hashtable[idx];
  while(traverse != NULL){
    cache_entry_t *cache_entry = traverse->cache_entry;
    if(strcmp(cache_entry->request, request) == 0){
      // Cache HIT
      // Move entry to front of list, join its neigbors
      if (cache_entry->next != NULL) {
        cache_entry->next->prev = cache_entry->prev;
      }
      if (cache_entry->prev != NULL) {
        cache_entry->prev->next = cache_entry->next;
      } else if (cache_entry->next != NULL){
        cache_info.rear = cache_entry->next;
      }
      cache_entry->next = NULL;
      if (cache_info.front != cache_entry){
        cache_info.front->next = cache_entry;
	cache_entry->prev = cache_info.front;
      }
      cache_info.front = cache_entry;
      return cache_entry;
    } else {
      traverse = traverse->collision;
    }
  }
  // Cache MISS
  return NULL;
}

// Function to add the request and its file content into the cache
void addIntoCache(char *request, char *content , int content_size){
  // It should add the request at an index according to the cache replacement policy
  // Make sure to allocate/free memeory when adding or replacing cache entries
  
  // Cache replacement policy LRU
  //printf("====================\nADDING TO CACHE\n");
  //printf("cache size and max: %d %d\n",cache_info.cache_size, max_cache_size);
  //printf("cache size: %d\n", cache_info.cache_size);
  if (cache_info.cache_size > max_cache_size - 1){
    // cache full, must evict last elem
    // printf("EVICTING REAR\n");
    // evict map entry
    int idx = hash_request(cache_info.rear->request);
    cache_map_entry_t *traverse = hashtable[idx];
    cache_map_entry_t *next_traverse = NULL;
    while(1){
      if(strcmp(cache_info.rear->request, traverse->cache_entry->request) == 0){
        cache_map_entry_t *prev_entry = traverse->collision;
        if (next_traverse == NULL){
          hashtable[idx] = prev_entry;
        } else {
          next_traverse->collision = prev_entry;
        }
	free(traverse);
        break;
      } else {
        next_traverse = traverse;
        traverse = traverse->collision;
      }
    }

    // evict cache entry
    if (cache_info.rear->next != NULL){
      cache_info.rear->next->prev = NULL;
    }
    cache_entry_t *newrear = cache_info.rear->next;
    free(cache_info.rear->request);
    free(cache_info.rear->content);
    free(cache_info.rear);
    cache_info.rear = newrear;
    cache_info.cache_size--;
  }
  // create new cache entry
  cache_entry_t *new_c_entry = malloc(sizeof(cache_entry_t));
  if (new_c_entry == NULL){
    perror("Failed to malloc a new cache entry: ");
    exit(1);
  }
  new_c_entry->request = request;
  new_c_entry->content = content;
  new_c_entry->content_size = content_size;
  new_c_entry->prev = cache_info.front;
  new_c_entry->next = NULL;
  if (cache_info.rear == NULL){
    cache_info.rear = new_c_entry;
  }
  if (cache_info.front != NULL){
    cache_info.front->next = new_c_entry;
  }
  cache_info.front = new_c_entry;
  cache_info.cache_size++;
  
  // create new map entry
  cache_map_entry_t *new_map_entry = malloc(sizeof(cache_map_entry_t));
  if (new_map_entry == NULL){
    perror("Failed to malloc a new map entry: ");
    exit(1);
  }
  new_map_entry->cache_entry = new_c_entry;
  new_map_entry->collision = NULL;
  int idx = hash_request(request);
  cache_map_entry_t *cur_map_entry = hashtable[idx];
  hashtable[idx] = new_map_entry;
  if(cur_map_entry != NULL){
    new_map_entry->collision = cur_map_entry;
  }
}

// clear the memory allocated to the cache
void deleteCache(){
  // De-allocate/free the cache memory
}

// Function to initialize the cache
void initCache(int cache_size){
  // Allocating memory and initializing the cache array
  max_cache_size = cache_size;
  max_hash_size = get_prev_prime(2 * cache_size);
  hashtable = malloc(sizeof(cache_map_entry_t*) * max_hash_size);
  if (hashtable == NULL){
    fprintf(stderr, "failed to malloc hashtable\n");
    exit(1);
  }
  cache_info.cache_size = 0;
  cache_info.front = NULL;
  cache_info.rear = NULL;
}

// Function to initialize request queue
void initRequestQueue(int buf_size){
  max_q_size = buf_size;
  request_q = malloc(sizeof(request_entry_t) * max_q_size);
  if (request_q == NULL) {
    fprintf(stderr, "failed to init request_q with malloc\n");
    exit(1);
  }
}

// Function to open and read the file from the disk into the memory
// Add necessary arguments as needed
int readFromDisk(char *filename, char **content, int *content_size) {
  // Open and read the contents of file given the request
  // Returns 1 on failure, 0 on success
  
  //TODO: finish error handling
  FILE *file;
  long filesize;
  char cwd[300];
  getcwd(cwd, 300);
  //printf("%s\n", cwd);
  
  file = fopen(filename, "r");
  if (file == NULL){
    return 1;
  }
  fseek(file, 0, SEEK_END);
  filesize = ftell(file);
  rewind(file);
  //printf("rewound\n");
  *content = malloc(filesize * sizeof(char));
  fread(*content, filesize, 1, file);
  //printf("read file in OK\n");
  fclose(file);
  *content_size = filesize;
  //printf("read from disk\n");
  return 0;
}

/**********************************************************************************/

/* ************************************ Utilities ********************************/
// Function to get the content type from the request
char* getContentType(char * mybuf) {
  // Should return the content type based on the file type in the request
  // (See Section 5 in Project description for more details)
  int buf_len = strlen(mybuf);
  char* ext;

  for(int i=0;i < buf_len; i++)
  {
    if(mybuf[i] == '.')
    {
      ext = &mybuf[i];
      break;
    }
  }

  if(strcmp(ext, ".html") == 0)
  {
    return "text/html";
  }
  else if(strcmp(ext, ".jpg") == 0)
  {
    return "image/jpeg";
  }
  else if(strcmp(ext, ".gif") == 0)
  {
    return "image/gif";
  }

  return "text/plain";
}

// This function returns the current time in microseconds
long getCurrentTimeInMicro() {
  struct timeval curr_time;
  if (gettimeofday(&curr_time, NULL) < 0){
    perror("Unable to obtain time of day.");
    exit(1);
  }
  return curr_time.tv_sec * 1000000 + curr_time.tv_usec;
}

void printCacheState(){
  printf("=================CACHE STATE=============\n");
  for(int i = 0; i < max_cache_size; i++){
   cache_map_entry_t *traverse = hashtable[i];
     if (traverse == NULL){
       printf("index: %d, ptr: %p\n", i, NULL);
     } else {
       do {
         printf("index: %d, ptr: %p\n", i, traverse);
	 traverse = traverse->collision;
       } while (traverse != NULL);
     }
  }
}

/**********************************************************************************/

// Function to receive the request from the client and add to the queue
void * dispatch(void *arg) {
  int fd;
  while (1) {
    usleep(5000);
    // Accept client connection
    fd = accept_connection();
    if (fd < 0){
      continue;
    }
    char *request = malloc(BUFF_SIZE * sizeof(char));
    if (request == NULL){
      perror("failed to malloc req buffer in dispatcher.");
      exit(1);
    }
    // Get request from the client
    if (get_request(fd, request) != 0){
      printf("bad request get_req\n");
      continue;
    }
    // Add the request into the queue
    pthread_mutex_lock(&q_lock);
    while (q_in_idx == q_out_idx && q_count == max_q_size){
      pthread_cond_wait(&q_not_full, &q_lock);
    }
    request_q[q_in_idx].fd = fd;
    request_q[q_in_idx].request = request;
    q_in_idx++;
    q_in_idx = q_in_idx % max_q_size;
    q_count++;
    pthread_mutex_unlock(&q_lock);
    pthread_cond_signal(&q_not_empty);
   }
   return NULL;
}

/**********************************************************************************/

void toLogAndTerminal(int threadID, int reqNum, int fd, char* reqStr, int bytes, int ttime, char* cacheHit)
{
  //Format: [threadID][reqNum][fd][Request String][bytes/error][time][Cache HIT/MISS]
  printf("[%d][%d][%d][%s][%d][%dms][%s]\n", threadID, reqNum, fd, reqStr, bytes, ttime, cacheHit);
  pthread_mutex_lock(&l_lock);
  fprintf(log_file, "[%d]{%d][%d][%s][%d][%dms][%s]\n", threadID, reqNum, fd, reqStr, bytes, ttime, cacheHit);
  fflush(log_file);
  pthread_mutex_unlock(&l_lock);
}

// Function to retrieve the request from the queue, process it and then return a result to the client
void * worker(void *arg) {
   int reqNum = 0;
   int threadID = *(int*) arg;
   free(arg);
   while (1)
   {
    int startTime = 0; int endTime = 0; int readStat = 0; int elapsedTime = 0;
    char* cacheHit;
    request_entry_t temp_req;
    cache_entry_t* temp_cent;
    
    // Get the request from the queue
    pthread_mutex_lock(&q_lock);
    while (q_in_idx == q_out_idx && q_count == 0)
    {
      // Since the queue is certainly empty, if there are extra workers, die
      if (dynamic_thread_flag && worker_count > min_worker_count){
        // TODO: LOG THIS
	printf("Removed an unneeded worker thread because the queue is empty.\n");
	worker_count--;
	pthread_mutex_unlock(&q_lock);
        pthread_exit(NULL);
      }
      pthread_cond_wait(&q_not_empty, &q_lock);
    }
    // Start recording time
    startTime = getCurrentTimeInMicro();
    temp_req = request_q[q_out_idx];
    
    q_out_idx++;
    q_out_idx = q_out_idx % max_q_size;
    q_count--;
    pthread_cond_signal(&q_not_full);
    pthread_mutex_unlock(&q_lock);

    // Get the data from the disk or the cache
    int content_size;
    char *request;
    char *content;
    pthread_mutex_lock(&c_lock);
    temp_cent = getCacheEntry(temp_req.request);
    if(temp_cent != NULL){
      content_size = temp_cent->content_size;
      request = temp_cent->request;
      content = temp_cent->content;
      cacheHit = "HIT";
    } else  {
      // request wasn't in cache, so we have to load from disk.
      // Give up the cache lock so the cache doesn't wait on disk I/O
      pthread_mutex_unlock(&c_lock);
      char *filename = malloc(BUFF_SIZE * (sizeof(char) + 1));
      if (filename == NULL){
        printf("Failed to malloc a filename\n");
        exit(1);
      }
      filename[0] = '.';
      request = temp_req.request;
      strcpy(&filename[1], temp_req.request);
      readStat = readFromDisk(filename, &content, &content_size);
      if (readStat == 0){
        // Only put files which are actually on the disk into the cache
        pthread_mutex_lock(&c_lock);
        if (getCacheEntry(temp_req.request) == NULL){
	  // Check if the cache got updated prior to obtaining the lock again
          addIntoCache(request, content, content_size);
        }
      }
      cacheHit = "MISS";
    }
    // Stop recording the time
    endTime = getCurrentTimeInMicro();
    elapsedTime = (endTime - startTime)/1000;
    reqNum++;
    // Log the request into the file and terminal
    toLogAndTerminal(threadID, reqNum, temp_req.fd, request, content_size, elapsedTime, cacheHit);

    // return the result
    if(readStat == 0) {
      if (return_result(temp_req.fd, getContentType(temp_req.request), content, content_size) < 0){
        printf("Failed to return result to requester.\n)");
        exit(1);
      }
    } else {
      char * errorMsg = "Failed to read from disk.";
      if (return_error(temp_req.fd, errorMsg) < 0){
        printf("Failed to return result to requester.\n");
        exit(1);
      }
    }
    // Give up the cache lock after sending the data back
    pthread_mutex_unlock(&c_lock);
   }
  return NULL;
}

/**********************************************************************************/

int main(int argc, char **argv) {
  // Error check on number of arguments
  // Bad argcount check
  if(argc != 8){
    printf("usage: %s port path num_dispatcher num_workers dynamic_flag queue_length cache_size\n", argv[0]);
    return -1;
  }
/*% ./web_server port path num_dispatch num_workers dynamic_flag qlen cache_entries
  argv[1] = port number 1025-65535
  argv[2] = path to web root location
  argv[3] = number of dispatcher threads
  argv[4] = number of worker threads
  argv[5] = flag for static or dynamic thread pool
  argv[6] = bounded length of request queue
  argv[7] = number of entries availible in cache (or max memory used by cache)
  */
  
  // Get the input args
  int port = atoi(argv[1]);  
  int num_dispatch = atoi(argv[3]);
  int num_workers = atoi(argv[4]);
  min_worker_count = num_workers;
  worker_count = num_workers;
  dynamic_thread_flag = atoi(argv[5]);
  int qlen = atoi(argv[6]);
  int cache_size = atoi(argv[7]);
  
  // Perform error checks on the input arguments
  if(port < 1025 || port > 65535){
    printf("invalid port");
    exit(1);
  }
  //create log file
  log_file = fopen("./web_server_log_file", "w+");
  if(log_file == NULL){
     perror("Failed to open log_file");
     return 1;
  }
  // Change the current working directory to server root directory
  if(chdir(argv[2])){
    perror("Failed to change directory");
    return 1;
  }


  // Initialize cache; initialize buffer; initialize server
  initCache(cache_size);
  initRequestQueue(qlen);
  init(port);

  // Create dispatcher and worker threads
  pthread_t thread;
  int error;

  int created_worker_count = num_workers;
  int *thread_id_ptr;
  for(int i=0;i < num_dispatch; i++){
    if(error = pthread_create(&thread, NULL, dispatch, NULL)){
      perror("Failed to create thread");
      return 1;
    }
    if(error = pthread_detach(thread)){
      perror("Failed to detach thread");
      return 1;
    }
  }
  for(int i=0;i < num_workers; i++){
    thread_id_ptr = malloc(sizeof(int));
    if (thread_id_ptr == NULL){
      printf("Failed to malloc thread_id_ptr\n");
      exit(1);
    }
    *thread_id_ptr = i;
    if(error = pthread_create(&thread, NULL, worker, thread_id_ptr)){
      perror("Failed to create thread");
      return 1;
    }
    if(error = pthread_detach(thread)){
      perror("Failed to detach thread");
      return 1;
    }
  }
  printf("Thread pools created\n");
  
  while(1) {
    usleep(10000);
    pthread_mutex_lock(&q_lock);
    //printf("q count: %d\nqlen: %d\n", q_count, qlen);
    //printf("current worker count: %d\n", worker_count);
    //printf("spawned %d workers\n", created_worker_count);
    if (dynamic_thread_flag && q_count == max_q_size){
      // spawn a new worker if queue load is high
      // TODO: LOG THIS
      printf("Created a worker thread because the queue is full.\n");
      thread_id_ptr = malloc(sizeof(int));
      if (thread_id_ptr == NULL){
        printf("Failed to realloc thread_id_ptr.\n");
        exit(1);
      }
      *thread_id_ptr = created_worker_count;
      if(error = pthread_create(&thread, NULL, worker, thread_id_ptr)){
        perror("Failed to create thread: ");
        return 1;
      }
      if(error = pthread_detach(thread)){
        perror("Failed to detach thread: ");
	return 1;
      }
      worker_count++;
      created_worker_count++;
    }
    pthread_mutex_unlock(&q_lock);
  }
  // Clean up
  fclose(log_file);
}
