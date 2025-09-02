# High-Performance Caching HTTP Proxy in C

A robust, multi-threaded, and high-performance caching HTTP proxy server written in C from the ground up. This project is engineered to serve as a scalable and efficient intermediary for web traffic, significantly reducing latency and bandwidth usage through an intelligent in-memory caching layer.[1]

***

## Table of Contents

- Overview
- Core Features
- System Architecture & Design Philosophy
- UML Component Diagram (Textual)
- Technical Deep Dive
- Getting Started
- Project File Structure
- Limitations & Future Improvements

***

## Overview

This proxy acts as a gateway between clients and the wider internet. Instead of connecting directly to a website, client software connects to the proxy server, which forwards the request and relays the response. The project elevates this classic middleware pattern through:

- **Multi-Threading:** Handling many users at once without blocking, each in a separate thread.
- **Caching:** Remembering frequently requested content and serving it instantly when requested again, without re-fetching it from the origin server.[2][1]

The request lifecycle:  
1. The main thread accepts new connections.  
2. Each is dispatched to a worker thread, which parses the HTTP request, checks the cache, and either serves from memory (cache hit) or fetches fresh data (cache miss).

***

## Core Features

- **Multi-Threaded Architecture:** Thread-per-connection model using POSIX `pthread`s—simple, robust, and highly effective for moderate to high concurrency.
- **High-Performance LRU Cache:** Implements a thread-safe, highly efficient Least Recently Used cache using a hash table plus a doubly-linked list for constant-time cache lookups and evictions.[2]
- **Concurrency Optimized:** Utilizes `pthread_rwlock_t` read-write locks, enabling unlimited readers and exclusive access during writes, critical for high-throughput cache operations under heavy load.
- **Clean Modular Design:** Core server, HTTP request parser, and the caching engine are separate and easily testable units, ensuring the codebase is extensible and maintainable.
- **Conditional Compilation:** The same codebase can be compiled with or without caching by toggling a Makefile flag, making it easy to test with/without the performance benefits of in-memory caching.

***

## System Architecture & Design Philosophy

This proxy server is built on modularity, performance, and robustness principles:

- **Encapsulation & Separation of Concerns:**  
  - *proxy_server*: Handles all socket and thread management, request/response routing.  
  - *proxy_parse*: Accurately decodes and reassembles HTTP protocol messages.  
  - *cache*: Manages fast and safe storage of frequently accessed objects.[4][5][3]

- **Design Choices Explained:**  
  - *Thread-per-Connection Model:*  
    - Simple to implement and reason about. Each client gets its own isolated thread, which increases stability and makes debugging straightforward.
    - Not suitable for extremely large numbers of simultaneous connections (see Limitations).
  - *Cache Engine:*  
    - A composite data structure (hash table + doubly-linked list) provides optimal performance: O(1) for lookups, inserts, removal, and LRU ordering.
  - *Synchronization Model:*  
    - A single mutex would force even read operations (which are the majority in a cache) to wait in line, drastically reducing throughput.
    - Read-write locks (`pthread_rwlock_t`) allow thousands of parallel reads, only locking out others during insertions or evictions, vastly improving scalability for real-world loads.

***

## UML Component Diagram (Textual)

```
+-----------------+         +------------------+       +-----------------+
| proxy_server    |-------> | proxy_parse      |------>|   cache         |
+-----------------+         +------------------+       +-----------------+
(Threads/Network)         (HTTP parser,         (hash table + LRU
                           header utilities)     cache engine)
```

***

## Technical Deep Dive

- **Cache Engine—Performance-Oriented LRU:**
  - *Hash Table*: Quickly maps a full request URL to a node in a doubly-linked list.
  - *Doubly-Linked List*: Tracks recency, with the head as MRU and tail as LRU for O(1) reordering and eviction.
  - *Concurrency*: Read-write locks guarantee unlimited concurrent `get` operations; only writes (insert or evict) require exclusive access.

- **Server Core—Sockets and Threading:**
  - Uses the Berkeley Sockets API:
    - `socket`, `setsockopt(SO_REUSEADDR)`, `bind`, `listen`, and `accept` are used to set up robust, non-blocking socket handling.
  - Each accepted client is handled in its own pthread (thread-per-connection model):
    - *Pros*: Simplicity, reliability, strong isolation.
    - *Cons*: Some resource inefficiency at extremely high scale compared to thread pools or event-driven models.

***

## Getting Started

### Prerequisites

- C compiler (gcc or clang)
- `make` tool
- A POSIX-compliant OS (Linux/macOS)

### Compilation

```bash
# Compile both the standard and caching-enabled executables
make all

# Build only the caching-enabled version
make proxy_server_with_cache

# Remove compiled binaries and object files
make clean
```

### Running the Server

```bash
# Run the caching server on port 8080
./proxy_server_with_cache 8080
```

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

- **Scalability:** The thread-per-connection model, while simple, does not scale to thousands of concurrent clients. For very high concurrency, an event-driven model (e.g., using `epoll`) or thread pool could be considered.
- **Protocol Coverage:** Only HTTP is supported; HTTPS (`CONNECT`) and HTTP/2 are not supported but could be added with further development.
- **Persistent Cache:** Current design is in-memory only; adding disk persistence or intelligent cache expiration could increase utility.
- **Monitoring & Logging:** More robust runtime diagnostics, usage statistics, and error logging could improve operation and troubleshooting.

***
