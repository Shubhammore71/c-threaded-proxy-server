# Multi-Threaded Caching HTTP Proxy Server in C

A high-performance, multi-threaded HTTP proxy server written in C. This project serves as a scalable middleman for web traffic, with an efficient LRU (Least Recently Used) cache to accelerate content delivery and reduce network latency. The design focuses on modularity, thread safety, and extensibility.

***

## Table of Contents

- Overview
- Features
- Architecture & Component Design
- UML/Class Diagram (Textual)
- Core Data Structures & Algorithms
  - LRU Cache
  - Concurrency (Thread/Lock Model)
- Module Descriptions
- Compilation & Running
- Testing the Proxy
- Project Structure
- Limitations & Extensibility

***

## Overview

This proxy server listens for HTTP requests from multiple clients, parses requests, forwards them to target servers, and caches responses for improved future access. Each connection is handled in a separate thread, and the LRU cache ensures only the most relevant objects remain in memory.

***

## Features

- **Multi-Threaded Architecture**: Each client connection is handled by a dedicated thread, offering true concurrency.
- **Efficient LRU Cache**: Employs a thread-safe, hash-table-backed doubly-linked list for O(1) cache operations and quick eviction.
- **Modular Design**: Segregates logic into server control, parsing, and caching modules for better maintainability.
- **Robust Concurrency**: Utilizes POSIX read-write locks (`pthread_rwlock_t`) for granular cache access, maximizing read concurrency.
- **Easy Build/Run**: Conditional compilation allows building with or without cache by toggling a flag in the Makefile.

***

## Architecture & Component Design

- **proxy_server.c**: The main entry point. Listens for connections and spawns a new pthread per client. Handles the accept loop and thread lifecycle.
- **proxy_parse.c/.h**: Implements parsing and reassembling for HTTP request lines/headers. Extracts method, path, protocol, and headers safely.
- **cache.c/.h**: Manages an in-memory LRU cache. Handles all object storage, lookup, eviction, and concurrency controls.

### UML/Class Diagram (Textual)

```
+------------------+        +-------------------+        +------------------+
|  proxy_server    | <----> |   proxy_parse     | <----> |      cache       |
+------------------+        +-------------------+        +------------------+
| accept(),        |        | parse/unparse/    |        | get/put,         |
| thread mgmt,     |        | manage headers    |        | eviction,        |
| socket I/O       |        +-------------------+        | hash+LRU logic   |
+------------------+                                         +------------------+
```

***

## Core Data Structures & Algorithms

### LRU Cache – Hash Table + Doubly-Linked List

- **Hash Table (`hash_table[]`)**:  
  - Each bucket points to a collision-handling list of `HashEntry`s.
  - Key: Full request URL.  
  - Value: Pointer to a `CacheNode`.

- **CacheNode Structure**:
  ```c
  typedef struct CacheNode {
      char *url;
      char *data;
      int size;
      struct CacheNode *prev, *next;
  } CacheNode;
  ```

- **Doubly-Linked List**:
  - Maintains recency of usage.
  - Head: Most recently used.
  - Tail: Least recently used (evicted first).

- **Eviction & Replacement**:
  - When inserting a new object that would exceed cache capacity, evict LRU nodes until sufficient space exists.
  - On cache hit, move node to head (MRU).

- **Concurrency**:
  - Reads use `pthread_rwlock_rdlock` (shared).
  - Writes and evictions use `pthread_rwlock_wrlock` (exclusive).
  - Granular locks allow many parallel reads with no blocking.

### Thread-Per-Connection Model

- Main server uses `accept()` in a loop.
- Each new client spawns a pthread, which:
  1. Reads/parses client HTTP request.
  2. Looks up response in cache (if enabled).
  3. If cached, returns stored object. If not cached or disabled, connects to origin server, forwards request, reads response, returns result, and adds to cache (if enabled).

***

## Module Descriptions

| Module           | Description                                                        |
|------------------|--------------------------------------------------------------------|
| proxy_server.c   | Server logic, client threads, sockets, and integration.            |
| proxy_parse.c/.h | HTTP request parsing, header management.                           |
| cache.c/.h       | Thread-safe LRU cache (hash table + doubly-linked list).           |
| Makefile         | Compilation rules; “with” and “without” cache.                     |
| README.md        | Project documentation (this file).                                 |

***

## Compilation & Running

### Requirements

- GCC/Clang (C99 or later)
- Linux/macOS
- POSIX Threads library

### Compiling

```bash
# Build both variants (with and without cache)
make all

# Only non-caching proxy
make proxy_server

# Only caching proxy
make proxy_server_with_cache
```

### Running

```bash
# Start caching proxy on port 8080
./proxy_server_with_cache 8080

# Or, non-caching proxy on port 9999
./proxy_server 9999
```

On startup, the server prints which port it is listening to and whether the cache is enabled.

***

## Testing the Proxy

1. **Configure System/Browser Proxy**:
   - Set HTTP proxy to `127.0.0.1` and the selected port (e.g., 8080).

2. **Access HTTP Sites** (HTTPS not supported in this build):
   - Example: http://example.com/, http://neverssl.com/
   - "Cache HIT" will display on repeat accesses if cache is enabled.

3. **Turn Off Proxy** when done to restore default browsing.

***

## Project Structure

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

## Limitations & Extensibility

- **Only supports HTTP**; HTTPS and CONNECT not implemented.
- **Thread-per-connection** is simple and robust but may not scale to very high concurrent client loads.
- **Easy to extend:**  
  - Add logging, statistics, further protocols (HTTPS), or persistent cache.
  - Swap to an event-driven model (select/epoll) for heavy concurrency use-cases.

***

## Reference: How It Works

1. Main server listens for connections.
2. Each connection is handed to a new thread.
3. Requests are parsed, and the cache is checked (if enabled).
4. Data is fetched/relayed/cached as appropriate; all resources are cleaned up after response.
5. Cache is protected by fine-grained locking for maximum concurrency.

***

**End of README**

[1](https://ppl-ai-file-upload.s3.amazonaws.com/web/direct-files/attachments/50519927/b8f73097-6447-4c87-b494-7778ae316af2/README.md)
[2](https://ppl-ai-file-upload.s3.amazonaws.com/web/direct-files/attachments/50519927/e09df5ad-df6b-4a82-8d02-b702663815b4/cache.c)
[3](https://ppl-ai-file-upload.s3.amazonaws.com/web/direct-files/attachments/50519927/03a91f72-e53e-4153-a9f5-1eb3621510b8/cache.h)
[4](https://ppl-ai-file-upload.s3.amazonaws.com/web/direct-files/attachments/50519927/dc473c42-cbbc-4847-9205-f0aa0734f691/proxy_parse.c)
[5](https://ppl-ai-file-upload.s3.amazonaws.com/web/direct-files/attachments/50519927/55676143-bcaf-4e8a-9195-192c75e6b081/proxy_parse.h)
[6](https://ppl-ai-file-upload.s3.amazonaws.com/web/direct-files/attachments/50519927/6c196302-95da-469a-8357-c09b67b6f8bb/proxy_server.c)
