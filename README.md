High-Performance Caching HTTP Proxy in C
A robust, multi-threaded, and high-performance caching HTTP proxy server written in C from the ground up. This project is engineered to serve as a scalable and efficient intermediary for web traffic, significantly reducing latency and bandwidth usage through an intelligent in-memory caching layer.

Project Overview
A proxy server acts as a gateway between a user's machine and the internet. Instead of connecting directly to a web server, a client connects to this proxy, which then forwards the request, retrieves the response, and sends it back. This project enhances this basic model by adding two critical features:

Multi-Threading: To handle many users at once without getting blocked.

Caching: To remember and instantly serve frequently requested content without re-fetching it from the internet.

Live Demo (Conceptual)
Imagine the server running in your terminal. A GIF would show the following flow:

First Request: A user requests http://example.com. The terminal shows a Cache MISS, the proxy fetches the page from the internet, adds it to the cache, and serves it.

Second Request: The user refreshes the page. The terminal instantly shows a Cache HIT, and the page is served directly from memory, demonstrating a significant speedup.

System Architecture and Design Philosophy
The server is built on a foundation of modularity, performance, and robustness. The core design separates concerns into distinct, independent modules, making the system easier to understand, maintain, and extend.

Request Lifecycle
A single client request flows through the system as follows:

Listen & Accept: The main server thread listens on a specified port. Upon receiving a new connection, it calls accept() and immediately dispatches the new client socket to a dedicated worker thread.

Dispatch: A new POSIX thread (pthread) is created specifically for this client. This isolates each client's request, preventing a slow request from blocking others.

Parse Request: The worker thread reads the raw HTTP request from the client socket and uses the proxy_parse module to decode it into a structured C object (ParsedRequest).

Check Cache: The request URL is used as a key to check the cache module.

Cache Hit: If the content exists in the cache and is valid, it is retrieved directly from memory and sent back to the client. The request is complete.

Cache Miss: If the content is not in the cache, the proxy proceeds to the next step.

Forward to Origin: The worker thread opens a new socket connection to the destination web server (the "origin"), forwards the client's HTTP request, and waits for a response.

Stream, Cache & Respond: As the response is received from the origin server, it is simultaneously streamed back to the client and, if it meets the criteria (e.g., size), stored in the cache for future requests.

Clean Up: The thread closes both the client and origin server sockets and terminates, freeing its resources.

UML Component Diagram
This diagram illustrates the primary components and their interactions:

Technical Deep Dive
This section details the critical engineering decisions that underpin the server's performance and stability.

The Cache Engine: A High-Performance LRU Implementation
A cache's value is determined by its speed. A slow cache can be worse than no cache at all. Our implementation is designed for maximum performance under concurrent load.

The Problem: The Cache Trilemma
A cache needs to perform three operations extremely quickly:

Find (Lookup): Check if an item exists.

Update (On Hit): Mark an item as "just used".

Evict (On Miss & Full): Find and remove the "least valuable" item.

The Solution: A Hash Table + Doubly-Linked List
This composite data structure is the canonical solution for an efficient LRU cache, providing O(1) average time complexity for all three operations.

Hash Table (cache_map): The hash table is the key to fast lookups.

Role: It maps a request URL (the key) to a direct memory pointer of a CacheNode in the linked list.

Performance: Hashing allows for near-instantaneous lookups, insertions, and deletions in O(1) on average.

Doubly-Linked List: The list is the key to tracking recency.

Role: It maintains the strict order of item usage. The head of the list is always the Most Recently Used (MRU) item, and the tail is the Least Recently Used (LRU) item.

Performance: Being doubly-linked (with prev and next pointers) is critical. When a cache hit occurs, we can pluck the node from its current position and move it to the head in O(1) time. When eviction is needed, we can remove the tail node in O(1) time.

Concurrency Model: Read-Write Locks (pthread_rwlock_t)
Choosing the correct synchronization primitive is critical for performance.

Why Not a Mutex? A simple mutex is a "pessimistic" lock. It assumes any access could be a write and grants exclusive access to only one thread at a time. In a read-heavy workload like a cache, this would create a massive bottleneck, forcing readers to wait in line unnecessarily.

Why a Read-Write Lock is Ideal: It's an "optimistic" lock. It understands the difference between read and write operations.

Shared Read Lock: It allows any number of threads to acquire a read lock simultaneously. This is perfect for our scenario, as multiple clients can get cache hits without blocking each other.

Exclusive Write Lock: When a thread needs to modify the cache (add a new item or evict an old one), it requests a write lock. The system waits for all readers to finish, grants exclusive access to the writer, and blocks all other threads until the write is complete. This maximizes concurrency while guaranteeing data integrity.

The Server Core: Sockets and Threading
Networking: The Sockets API
The server is built on the standard Berkeley Sockets API. The main thread follows the canonical server setup:

socket(): Creates the network endpoint.

setsockopt(SO_REUSEADDR): A crucial step that allows the server to restart immediately after being shut down, avoiding "Address already in use" errors.

bind(): Assigns the specified port to the socket.

listen(): Puts the socket in a passive mode, ready to accept incoming connections.

accept(): Blocks until a client connects, then returns a new socket for that specific connection.

Concurrency Model: Thread-per-Connection
How it Works: For every incoming connection, a new POSIX thread is spawned to handle it.

Advantages:

Simplicity: The logic is straightforward, and each thread manages its own state.

Isolation: A crash or error in one client's thread will not affect others.

Limitations & Trade-offs: This model is excellent for learning and for moderate loads, but it does not scale to thousands of simultaneous connections due to the overhead of creating and context-switching threads. A more advanced architecture would use a thread pool or an event-driven model (e.g., using epoll or kqueue) to handle massive scale.

Getting Started
Prerequisites
A C compiler (like gcc or clang)

make build automation tool

A POSIX-compliant OS (Linux, macOS)

Compilation
A Makefile simplifies the entire compilation process.

# To compile both the standard and caching-enabled executables
make all

# To build only the version WITH the cache
make proxy_server_with_cache

# To remove all compiled binaries and object files
make clean

Running the Server
Run the desired executable, providing a port number as a command-line argument.

# Run the high-performance caching server on port 8080
./proxy_server_with_cache 8080

# The server will confirm it's running:
# > Cache enabled.
# > Proxy server listening on port 8080...
