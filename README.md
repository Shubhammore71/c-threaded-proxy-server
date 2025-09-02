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
