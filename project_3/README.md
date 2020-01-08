/* CSci4061 F2018 Assignment 3

Group number: 5
date: 12/10/2018
name: Lee Wintervold, Jack Bartels, Joe Keene
id: Winte413, Barte285, Keen0129 */

This project implements a multithreaded web server which responds to HTTP GET requests. The server first initializes a bounded
buffer to hold worker requests and and LRU cache to store commonly requested data in memory. The server then spawns a number
of worker and dispatcher threads as specified by the command line arguments. Dispatcher threads repeatedly check for new
connections and if one exists, puts the request onto the bounded buffer. Dispatcher threads will block if the buffer is full.
Worker threads repeatedly check the bounded buffer for requests genereated by the dispatcher threads, and if one exists will
attempt to service the request by checking the cache. If the entry is in the cache, the data is returned to the requester. If
the data is not in the cache, the data is retrieved from disk and placed into the cache. A log file is maintained which logs
each request processed, the worker that handled it, the number of requests that worker has handled, the total bytes sent,
the time taken for the worker to process the request, and whether the request was present in the cache. 

LRU cache - The least recently used cache entry is evicted when the cache is full and a new entry is put in.

Dynamic thread pool - The server checks the size of the request buffer occasionally. If the buffer is full, a new worker is
spawned. When each worker thread checks the buffer for new work, if the buffer is empty and there are more threads than
initially specified, the worker dies.

Lee - I worked on the LRU cache, bounded buffer, dispatcher threads, worker threads, reading from disk, and the dynamic thread
pool implementation.
Joe - I worked on the thread pools, argument handling and logging. 
Jack - I worked on the worker threads, getContentType utility function, and the performance analysis report.

To compile, run make.

Starting the server: ./web_server 12002 ./testing <num_dispatch> <num_worker> <dynamic_flag> <queue_len> <cache_entries>

Once the server is running, files can be downloaded via wget like so:

wget http://127.0.0.1:12002/image/jpg/29.jpg

If you have a file which contains all the URLs you wish to download (./testing/urls for example), you can use xargs to 
download multiple files at a time:

cat <path _to_urls_file> | xargs -n <number of args to each process> -P <number of processes to run> wget
