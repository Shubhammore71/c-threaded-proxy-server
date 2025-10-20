/**
 * @file cache.h
 * @author Shubham More
 * @brief Interface for a thread-safe, in-memory LRU cache.
 *
 * This module provides a cache to store HTTP responses. It uses a hash table
 * for fast lookups and a doubly-linked list to maintain the
 * least-recently-used (LRU) order for efficient eviction. The cache is
 * thread-safe, using read-write locks for synchronization.
 */

#ifndef CACHE_H
#define CACHE_H

#include <stddef.h>
#include <time.h>

/**
 * @brief Initializes the cache with specified limits.
 *
 * This function must be called before any other cache function.
 * @param max_size The maximum total size of all objects in the cache (in bytes).
 * @param max_element_size The maximum size of a single object (in bytes).
 * @return 0 on success, -1 on failure.
 */
int cache_init(size_t max_size, size_t max_element_size);

/**
 * @brief Frees all resources used by the cache.
 *
 * This should be called on graceful shutdown to prevent memory leaks.
 */
void cache_destroy();

/**
 * @brief Retrieves an object from the cache.
 *
 * If the object is found, it is marked as recently used.
 * @param url The URL key for the object.
 * @param size Pointer to a variable that will be filled with the object's size.
 * @return A pointer to the cached data if found, otherwise NULL.
 * @note The returned data is a copy and must be freed by the caller.
 */
char* cache_get(const char *url, int *size);

/**
 * @brief Adds an object to the cache.
 *
 * If the cache is full, the least recently used objects are evicted to make space.
 * If the object is larger than the max element size, it is not cached.
 * @param url The URL key for the object.
 * @param data The data to be cached.
 * @param size The size of the data.
 */
void cache_put(const char *url, const char *data, int size);

#endif
