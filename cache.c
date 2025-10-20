/**
 * @file cache.c
 * @author Shubham More
 * @brief Implementation of a thread-safe, in-memory LRU cache.
 *
 * This file contains the logic for the LRU cache. It uses a hash table
 * for O(1) average time complexity lookups and a doubly-linked list to
 * manage the LRU order. Thread safety is ensured using POSIX read-write locks.
 */

#include "cache.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>

#define HASH_TABLE_SIZE 1024 // Power of 2 for efficient modulo

// A node in the doubly-linked list (for LRU tracking)
typedef struct CacheNode {
    char *url;
    char *data;
    int size;
    struct CacheNode *prev;
    struct CacheNode *next;
} CacheNode;

// An entry in the hash table
typedef struct HashEntry {
    char *url;
    CacheNode *node;
    struct HashEntry *next; // For handling hash collisions
} HashEntry;

// --- Global Cache State ---
static size_t MAX_CACHE_SIZE;
static size_t MAX_ELEMENT_SIZE;
static size_t current_cache_size = 0;

// The main cache data structures
static HashEntry *hash_table[HASH_TABLE_SIZE];
static CacheNode *lru_head = NULL;
static CacheNode *lru_tail = NULL;

// Synchronization primitive
static pthread_rwlock_t cache_lock;

// --- Private Helper Functions ---

/**
 * @brief Simple hash function for URLs.
 */
static unsigned int hash(const char *url) {
    unsigned int hash_val = 0;
    for (; *url; ++url) {
        hash_val = (hash_val << 5) + *url;
    }
    return hash_val % HASH_TABLE_SIZE;
}

/**
 * @brief Detaches a node from the LRU list.
 */
static void lru_detach(CacheNode *node) {
    if (node->prev) node->prev->next = node->next;
    else lru_head = node->next;
    
    if (node->next) node->next->prev = node->prev;
    else lru_tail = node->prev;
}

/**
 * @brief Attaches a node to the front of the LRU list (most recently used).
 */
static void lru_attach(CacheNode *node) {
    node->next = lru_head;
    node->prev = NULL;
    if (lru_head) lru_head->prev = node;
    lru_head = node;
    if (lru_tail == NULL) lru_tail = node;
}

/**
 * @brief Frees a cache node and its contents.
 */
static void free_node(CacheNode *node) {
    if (node) {
        free(node->url);
        free(node->data);
        free(node);
    }
}

/**
 * @brief Evicts the least recently used elements until there is enough space.
 */
static void evict(size_t space_needed) {
    while (current_cache_size + space_needed > MAX_CACHE_SIZE && lru_tail) {
        CacheNode *node_to_evict = lru_tail;
        
        // Remove from hash table
        unsigned int index = hash(node_to_evict->url);
        HashEntry *entry = hash_table[index];
        HashEntry *prev = NULL;
        while (entry) {
            if (strcmp(entry->url, node_to_evict->url) == 0) {
                if (prev) prev->next = entry->next;
                else hash_table[index] = entry->next;
                free(entry->url);
                free(entry);
                break;
            }
            prev = entry;
            entry = entry->next;
        }
        
        // Remove from LRU list
        lru_detach(node_to_evict);
        current_cache_size -= node_to_evict->size;
        
        // Free the node itself
        printf("Cache Evict: %s\n", node_to_evict->url);
        free_node(node_to_evict);
    }
}


// --- Public API Functions ---

/**
 * @brief Initializes the cache.
 */
int cache_init(size_t max_size, size_t max_element_size) {
    MAX_CACHE_SIZE = max_size;
    MAX_ELEMENT_SIZE = max_element_size;
    
    if (pthread_rwlock_init(&cache_lock, NULL) != 0) {
        perror("Failed to initialize cache lock");
        return -1;
    }

    memset(hash_table, 0, sizeof(hash_table));
    return 0;
}

/**
 * @brief Destroys the cache, freeing all memory.
 */
void cache_destroy() {
    pthread_rwlock_wrlock(&cache_lock);
    
    CacheNode *current = lru_head;
    while (current) {
        CacheNode *next = current->next;
        free_node(current);
        current = next;
    }
    
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashEntry *entry = hash_table[i];
        while (entry) {
            HashEntry *next = entry->next;
            free(entry->url);
            free(entry);
            entry = next;
        }
    }
    
    pthread_rwlock_unlock(&cache_lock);
    pthread_rwlock_destroy(&cache_lock);
}

/**
 * @brief Retrieves an item from the cache.
 */
char* cache_get(const char *url, int *size) {
    pthread_rwlock_rdlock(&cache_lock);

    unsigned int index = hash(url);
    HashEntry *entry = hash_table[index];
    
    while (entry) {
        if (strcmp(entry->url, url) == 0) {
            // Found it. Now upgrade lock to write to modify LRU list.
            pthread_rwlock_unlock(&cache_lock);
            pthread_rwlock_wrlock(&cache_lock);

            // Need to re-check if it's still there after re-acquiring lock
            // This is a common pattern: optimistic read, then write lock for modification
            entry = hash_table[index]; // Re-start search from bucket head
            while (entry) {
                 if (strcmp(entry->url, url) == 0) {
                     // Still there, proceed.
                     lru_detach(entry->node);
                     lru_attach(entry->node);
                     
                     char *data_copy = malloc(entry->node->size);
                     if (data_copy) {
                         memcpy(data_copy, entry->node->data, entry->node->size);
                         *size = entry->node->size;
                     }
                     pthread_rwlock_unlock(&cache_lock);
                     return data_copy;
                 }
                 entry = entry->next;
            }
            // If we are here, item was deleted between lock release/acquire
            pthread_rwlock_unlock(&cache_lock);
            return NULL;
        }
        entry = entry->next;
    }

    pthread_rwlock_unlock(&cache_lock);
    return NULL;
}

/**
 * @brief Adds an item to the cache.
 */
void cache_put(const char *url, const char *data, int size) {
    if (size > MAX_ELEMENT_SIZE) {
        return; // Item is too large to cache
    }

    pthread_rwlock_wrlock(&cache_lock);

    // First, check if item already exists. If so, update it.
    unsigned int index = hash(url);
    HashEntry *entry = hash_table[index];
    while (entry) {
        if (strcmp(entry->url, url) == 0) {
            // It exists. Just update data and size, and move to front.
            free(entry->node->data);
            entry->node->data = malloc(size);
            if (!entry->node->data) { /* Handle malloc failure */
                pthread_rwlock_unlock(&cache_lock);
                return;
            }
            memcpy(entry->node->data, data, size);
            current_cache_size -= entry->node->size;
            current_cache_size += size;
            entry->node->size = size;

            lru_detach(entry->node);
            lru_attach(entry->node);
            
            // Check if we need to evict after update
            evict(0);

            pthread_rwlock_unlock(&cache_lock);
            return;
        }
        entry = entry->next;
    }


    // Item does not exist. Create new node and add it.
    evict(size);

    CacheNode *new_node = malloc(sizeof(CacheNode));
    HashEntry *new_entry = malloc(sizeof(HashEntry));

    if (!new_node || !new_entry) {
        free(new_node); free(new_entry);
        pthread_rwlock_unlock(&cache_lock);
        return;
    }

    new_node->url = strdup(url);
    new_node->data = malloc(size);
    if (!new_node->url || !new_node->data) {
        free(new_node->url); free(new_node->data);
        free(new_node); free(new_entry);
        pthread_rwlock_unlock(&cache_lock);
        return;
    }
    memcpy(new_node->data, data, size);
    new_node->size = size;

    // Add to LRU list
    lru_attach(new_node);

    // Add to hash table
    new_entry->url = strdup(url);
    new_entry->node = new_node;
    new_entry->next = hash_table[index];
    hash_table[index] = new_entry;

    current_cache_size += size;
    printf("Cache Add: %s, Size: %d, Total Cache Size: %zu\n", url, size, current_cache_size);

    pthread_rwlock_unlock(&cache_lock);
}
