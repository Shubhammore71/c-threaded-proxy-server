# Multi-Threaded Caching HTTP Proxy Server in C

A high-performance, multi-threaded caching HTTP proxy server written entirely in C. This project is designed to be a robust, scalable middleman for web traffic, featuring a highly efficient LRU cache to accelerate content delivery and reduce network latency.

***

## Table of Contents

- Overview
- Core Features
- System Architecture & Design Philosophy
- UML Class Diagram (Detailed)
- Technical Deep Dive
- Getting Started
- Project File Structure
- Limitations & Future Improvements

***

## Overview

This proxy server acts as an intermediary between client machines and web servers. Instead of clients connecting directly to websites, they connect to the proxy, which forwards requests, fetches responses, and can cache results for instant subsequent delivery. Two core features boost performance:

- **Multi-Threading:** Handle many concurrent clients, each with its own thread.
- **Caching:** In-memory LRU cache instantly serves frequently requested content.

***

## Core Features

- **Multi-Threaded Architecture:** Thread-per-connection model with POSIX threads (`pthread`). Each client is handled independently and concurrently.
- **High-Performance LRU Cache:** Employs a thread-safe combination of a hash table and a doubly-linked list to achieve O(1) average time for all cache operations.
- **Robust Concurrency:** Uses `pthread_rwlock_t` (read-write locks) enabling unlimited concurrent reads with safe, exclusive writes during cache mutations.
- **Modular, Clean Codebase:** The server, HTTP parser, and cache modules are clearly separated for maintainability and ease of extension.
- **Conditional Compilation:** Use the Makefile flag to include or exclude caching for performance benchmarking and debugging.

***

## System Architecture & Design Philosophy

The codebase is modular and robust, with clear separation of concerns:

- **proxy_server:** Accepts connections, manages each client in a separate worker thread, and orchestrates the data flow.
- **proxy_parse:** Responsible for parsing and rebuilding HTTP request lines and headers.
- **cache:** A thread-safe in-memory LRU cache with hash-table lookup and doubly-linked list eviction order.

***
Here is the requested section, formatted and integrated as an addition to your original README file. It fits naturally after the "System Architecture & Design Philosophy" or before the "Technical Deep Dive" section to highlight the high-level design and data flow clearly for interviews or presentations:

***

## High-Level Architecture Diagram & Request Flow


### High-Level Architecture Diagram

```
+-----------------+      (1) TCP      +-------------------------------------------------------+      (8) DNS      +------------------+
|                 |    Connection     |                                                       |     Lookup      |                  |
|  Client Browser |<----------------->|                    PROXY SERVER                     |<--------------->|   DNS Server     |
| (e.g., Chrome)  |                   |                                                       |                 |                  |
+-----------------+                   | +-----------------+       +-----------------------+ |                 +------------------+
                                      | |                 |       |                       | |
                                      | |   Main Thread   |------>|     Worker Thread     | |      (9) TCP      +------------------+
                                      | | (Listen/Accept) |(3)    |   (Handle Request)    |<----------------->|                  |
                                      | |                 |Spawns |                       | |    Connection     |  Origin Server   |
                                      | +-----------------+       +-----------+-----------+ |                 | (e.g., example.com)|
                                      |                             (4) |       ^ (7a)      |                 |                  |
                                      |                                 v       |           |                 +------------------+
                                      | +-----------------------+   +-----------+-----------+
                                      | |                       |   |                       |
                                      | |      HTTP Parser      |<--|       Cache Check     |
                                      | |    (proxy_parse.c)    |   |      (Decision)       |
                                      | |                       |   |                       |
                                      | +-----------------------+   +-----------+-----------+
                                      |                             (5) |       ^ (6b)      |
                                      |                                 v       |           |
                                      | +---------------------------------------+-----------+
                                      | |                                                   |
                                      | |                  Cache Module (cache.c)           |
                                      | |                                                   |
                                      | | +-----------------+   +-------------------------+ |
                                      | | |   Hash Table    |-->| Doubly-Linked List (LRU)| |
                                      | | | (Fast Lookups)  |   |     (Recency Order)     | |
                                      | | +-----------------+   +-------------------------+ |
                                      | |       [Protected by pthread_rwlock_t]             |
                                      | +---------------------------------------------------+
                                      |                                                       |
                                      +-------------------------------------------------------+
```

### Detailed Walkthrough of the Request Flow

1. **The Initial Connection**

   What happens: A user's web browser, configured to use our proxy, initiates a TCP connection to the proxy server's IP address and port (e.g., 127.0.0.1:8080). The OS handles the three-way handshake (SYN, SYN-ACK, ACK) to establish a stable connection.

2. **The Main Thread: The Listener**

   What happens: The main function in `proxy_server.c` runs an infinite `while(1)` loop. Its only job is to call the blocking function `accept()`. It waits patiently at this line until a new client connection, like the one from step 1, arrives.

   Design Choice: The main thread does no heavy lifting. It doesn't parse, it doesn't cache, it doesn't connect to the internet. It is purely a delegator. This is a key design principle for a responsive server—the main thread must always be free to accept new connections immediately.

3. **Spawning a Worker Thread**

   What happens: As soon as `accept()` returns a new socket for the client, the main thread immediately calls `pthread_create()`. This creates a brand new, independent thread of execution. The new thread is given the `handle_connection` function as its starting point and the client's socket descriptor as its argument.

   Architectural Model: This is the "thread-per-connection" model. It provides excellent isolation—if one worker thread crashes, it won't affect any other client. The trade-off is the overhead of creating a new thread for every single request.

4. **Parsing the HTTP Request**

   What happens: The newly spawned Worker Thread now takes over. It reads the raw, plain-text HTTP request from the client's socket (e.g., `GET http://example.com/ ...`). It then passes this raw string to the HTTP Parser module (`proxy_parse.c`).

   Component Interaction: The parser's job is to act as a translator, turning the unstructured text into a clean `ParsedRequest` C struct. This encapsulates the parsing logic, keeping the main server code clean.

5. **The Cache Check (The Critical Decision)**

   What happens: The worker thread takes the parsed URL (e.g., `http://example.com/`) and uses it as a key to query the Cache Module. This is the most important decision point in the entire flow.

   Path A (Cache Hit) or Path B (Cache Miss)? The result of this check determines whether we take the "fast path" or the "slow path."

6. **The Fast Path: Cache Hit**

   What happens: The Cache Module's hash table provides an O(1) lookup and finds the content. The worker thread then:

   - (a) Retrieves the cached web object (the HTTP response) directly from memory.
   - (b) Updates the cache's LRU state by moving the corresponding node in the doubly-linked list to the very front (making it the Most Recently Used).
   - (c) Sends the cached response directly to the Client Browser. The request is complete.

   Performance: This entire process avoids any new network connections and is extremely fast. This is the primary purpose of the proxy.

7. **The Slow Path: Cache Miss**

   What happens: The Cache Module reports that the requested object is not in memory. The worker thread must now act as a regular web client.

8. **DNS Lookup**

   What happens: The worker thread extracts the hostname (e.g., `example.com`) from the parsed request and calls a function like `gethostbyname()` to ask the OS to resolve this name into an IP address. This may involve a network call to a DNS Server.

9. **Forwarding the Request to the Origin Server**

   What happens: Using the IP address from the DNS lookup, the worker thread opens a new TCP connection to the Origin Server. It then forwards the original HTTP request it received from the client.

   Key Role: At this moment, the proxy is simultaneously a server (to the client browser) and a client (to the origin server).

10. **Receiving and Caching the Response**

    What happens: The worker thread receives the HTTP response from the Origin Server. As the data streams in, it does two things in parallel:

    - (a) Forward: It immediately forwards the data back to the waiting Client Browser.
    - (b) Store: It simultaneously stores a copy of the response in the Cache Module by calling `cache_put()`. If the cache is full, this action will trigger the eviction of the Least Recently Used item at the tail of the linked list.

***

## UML Class Diagram

**Textual Diagram:**

```
+--------------------------+                +----------------------+
|     proxy_server.c       |                |   proxy_parse.c/.h   |
+--------------------------+                +----------------------+
| - main()                 |                | - ParsedRequest      |
| - handle_connection()    |<----+    +---->| - ParsedHeader       |
| - forward_request...()   |     |    |     |----------------------|
| - send_error_to_client() |     |    |     | + ParsedRequest_create()    
| - signal_handler()       |     |    |     | + ParsedRequest_parse()
|--------------------------|     |    |     | + ParsedRequest_destroy()
| - thread_args_t          |     |    |     | + ParsedRequest_unparse()
|--------------------------|     |    |     | + ParsedHeader_set()
| + Accepts connections    |     |    |     | + ParsedHeader_get()
| + Launches pthreads      |     |    |     | + ParsedHeader_remove()
| + One thread per client  |     |    |     +----------------------+
+--------------------------+     |    |
  |                             Worker thread gets HTTP request, parses with proxy_parse
  |                                  |
  |                                  v
+----------------------------------+ |    +-----------------------+
|            cache.c/.h            |----->|   cache_lock (rwlock) |
+----------------------------------+      +-----------------------+
| - cache_init(), cache_destroy()  |      | Synchronizes all      |
| - cache_get(), cache_put()       |      | cache access (RO/RW)  |
| - Internal:                      |      +-----------------------+
|   - HASH_TABLE_SIZE              |
|   - hash_table[]                 |
|   - lru_head, lru_tail           |
|   - pthread_rwlock_t cache_lock  |
|   - CacheNode                    |
|   - HashEntry                    |
+----------------------------------+
| + Manages objects by URL         |
| + Handles LRU ordering & eviction|
| + Ensures thread safety          |
+----------------------------------+
```

### Data Types & Relationships

| Module           | Structure        | Major Fields                             | Description                         |
|------------------|------------------|------------------------------------------|-------------------------------------|
| proxy_server.c   | thread_args_t    | client_socket_fd                         | Passes fd to worker thread          |
| proxy_parse.h    | ParsedRequest    | method, protocol, host, port, path, ...  | Parsed HTTP request abstraction     |
| proxy_parse.h    | ParsedHeader     | key, value                               | Header storage                      |
| cache.c          | CacheNode        | url, data, size, *prev, *next            | DLL for LRU, cache entry            |
| cache.c          | HashEntry        | url, *node, *next                        | Hash table linkage for cache nodes  |
| cache.c          | hash_table[]     | Array of HashEntry *                     | Fast lookup by URL hash             |
| cache.c          | pthread_rwlock_t | cache_lock                               | Multi-reader/single-writer lock     |

**How it Fits Together:**
- `proxy_server.c` listens and dispatches to threads (each handling a client).
- Each worker parses requests (`proxy_parse.c/.h`), checks/inserts into the cache layer, and relays or returns responses.
- All cache operations are guarded by `cache_lock`, allowing efficient parallel reads and serialized writes.
- The server is gracefully shut down via `signal_handler`, which ensures all resources are cleaned up safely.

***

## Technical Deep Dive

### The LRU Cache

- **Hash Table:** Maps URL strings (keys) to corresponding cache nodes for O(1) access.
- **Doubly-Linked List:** Maintains LRU order as nodes are accessed/added/evicted.
- **Thread Safety:** `pthread_rwlock_t` lets many threads read the cache in parallel, with isolated write locking for cache insertions or evictions.
- **Workflow:**  
  1. On a cache GET: search hash table.  
  2. On a hit, move node to list head (most recently used) in O(1).
  3. On a miss, fetch from origin, insert. If needed, evict from tail (least recently used).

### The Server Core

- Accepts TCP connections, runs a worker per connection.
- Workers parse HTTP requests, interact with cache, manage upstream connection and response relay.
- On termination or signal, the system ensures all threads are detached/joined, and all resources (e.g., mutexes, cache entries) are freed cleanly.

***

## Getting Started

### Prerequisites

- C compiler (GCC/Clang)
- `make`
- POSIX compliant OS (Linux/macOS)

### Compilation

```bash
# Build all variants
make all

# Only build caching version
make proxy_server_with_cache
```

### Running

```bash
# Run cache-enabled server on port 8080
./proxy_server_with_cache 8080

# Without cache, on port 9999
./proxy_server 9999
```

***

## Testing

- Configure system/browser to use `127.0.0.1:<port>` as HTTP proxy.
- Test with sites such as http://example.com or http://neverssl.com.
- Watch the terminal for logs: "Cache MISS" for new requests, "Cache HIT" for repeated requests.

***

## Project File Structure

```
.
├── Makefile
├── README.md
├── cache.c
├── cache.h
├── proxy_parse.c
├── proxy_parse.h
└── proxy_server.c
```

***

## Limitations & Future Improvements

- **Thread-per-connection** scales well for moderate load but can be upgraded to a thread pool or event-driven (epoll) model for extreme concurrency.
- **Only supports HTTP**, not HTTPS or HTTP/2 (could be extended).
- **Cache is in-memory only**; persistent or disk-backed options could be added.
- **Extensible logging, monitoring**, and runtime configuration would aid in production deployments.

***
