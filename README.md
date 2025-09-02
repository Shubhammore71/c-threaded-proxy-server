Multi-Threaded Caching HTTP Proxy Server in C
A high-performance, multi-threaded caching HTTP proxy server written entirely in C. This project is designed to be a robust, scalable middleman for web traffic, featuring a highly efficient LRU cache to accelerate content delivery and reduce network latency.

Key Features
Multi-Threaded Architecture: Utilizes a "thread-per-connection" model to handle multiple concurrent clients without blocking.

High-Performance LRU Cache: Implements a thread-safe, Least Recently Used (LRU) cache to store and serve frequently accessed web objects, drastically speeding up response times.

Modular Design: Code is cleanly separated into a server core, an HTTP request parser, and a cache module for maintainability and clarity.

Robust Concurrency: Employs pthread read-write locks (pthread_rwlock_t) for fine-grained cache access, maximizing concurrency by allowing simultaneous reads.

Conditional Compilation: A single codebase can be compiled with or without the caching feature using a Makefile flag.

Architecture & Design
The proxy server is built on a modular architecture that separates distinct responsibilities into their own components. The main thread listens for incoming connections and dispatches each one to a new worker thread, ensuring the server remains responsive.

UML Class Diagram
This diagram illustrates the relationship between the main components of the server.

The proxy_server is the main entry point, responsible for networking and thread management.

It uses the proxy_parse module to decode raw HTTP requests.

If caching is enabled, it interacts with the cache module to store and retrieve web objects.

Core Data Structures and Algorithms
The performance of this proxy hinges on the efficiency of its core data structures and algorithms.

1. The Cache Module
Data Structure: Hash Table + Doubly-Linked List

To implement the LRU policy, the cache uses the classic combination of a hash table and a doubly-linked list.

Hash Table (cache_map): Provides near O(1) average-case time complexity for lookups. The key is the request URL, and the value is a direct pointer to a node in the linked list.

Doubly-Linked List: Maintains the order of items by recency. The head of the list is the most recently used item, and the tail is the least recently used. Its doubly-linked nature allows for O(1) time complexity when moving a node to the front (on a cache hit) or removing the last node (on an eviction).

Algorithm: Least Recently Used (LRU) Cache Replacement

When the cache is full and a new item needs to be added, the item at the tail of the linked list (the one that hasn't been accessed for the longest time) is evicted.

Concurrency: pthread_rwlock_t (Read-Write Lock)

The cache is protected by a read-write lock, which is significantly more performant than a simple mutex for this workload. It allows any number of threads to read from the cache simultaneously, only locking exclusively when a thread needs to write (add or evict an item).

2. The Server Core
Concurrency Model: Thread-per-Connection

This model is straightforward to implement. The main thread's only job is to accept() new connections and spawn a new pthread to handle the entire lifecycle of that client's request. This isolates clients from one another but can be resource-intensive at massive scale.

How to Compile and Run
A Makefile is provided for easy compilation on Linux or macOS.

1. Compile
You can compile both the caching and non-caching versions of the server.

# To compile both executables
make all

# To compile only the version WITHOUT the cache
make proxy_server

# To compile only the version WITH the cache
make proxy_server_with_cache

2. Run
After compiling, run the desired executable followed by a port number.

# Run the server WITH caching on port 8080
./proxy_server_with_cache 8080

# Or, run the server WITHOUT caching on port 9999
./proxy_server 9999

The server will then print a confirmation message, e.g., Proxy server listening on port 8080....

How to Test
To test the proxy, you must configure your web browser or operating system to use it.

Open System/Browser Proxy Settings:

On macOS: System Settings > Network > Wi-Fi > Details... > Proxies

On Windows: Settings > Network & Internet > Proxy

In Firefox: Settings > General > Network Settings > Settings...

Configure Manually:

Select "Manual proxy configuration".

HTTP Proxy: 127.0.0.1

Port: The port you used to run the server (e.g., 8080).

Browse the Web:

Visit an HTTP website to see the proxy in action (HTTPS requires additional handling not implemented in this version).

Good test sites: http://example.com or http://neverssl.com.

You will see detailed log messages in your server's terminal window. Refreshing the page should show a Cache HIT.

IMPORTANT: Remember to disable the proxy settings in your browser/OS when you are finished testing!

Project File Structure
.
├── Makefile
├── README.md
├── cache.c
├── cache.h
├── proxy_parse.c
├── proxy_parse.h
└── proxy_server.c
