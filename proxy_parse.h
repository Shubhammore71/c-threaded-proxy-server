/**
 * @file proxy_parse.h
 * @author Shubham More
 * @brief Interface for a simple HTTP request parser.
 *
 * This file defines the structures and functions for parsing raw HTTP requests.
 * It is designed to extract key information from a request line and manage
 * HTTP headers. The implementation is in proxy_parse.c.
 *
 * Based on the original proxy_parse.h by Matvey Arye.
 */

#ifndef PROXY_PARSE_H
#define PROXY_PARSE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>

// Set to 1 to enable debug prints to stderr, 0 to disable.
#define DEBUG 1

/**
 * @struct ParsedHeader
 * @brief Represents a single HTTP header (key-value pair).
 *
 * Headers are stored as a dynamically-sized array within ParsedRequest.
 */
struct ParsedHeader {
    char *key;
    char *value;
};

/**
 * @struct ParsedRequest
 * @brief Represents a parsed HTTP request.
 *
 * Contains fields for the request line components (method, protocol, host, etc.)
 * and a list of headers.
 */
struct ParsedRequest {
    char *method;
    char *protocol;
    char *host;
    char *port;
    char *path;
    char *version;
    char *buf; // Original request buffer
    size_t buflen;
    struct ParsedHeader *headers;
    size_t headers_used;
    size_t headers_len;
};

/**
 * @brief Creates a new, empty ParsedRequest object.
 * @return A pointer to the newly allocated ParsedRequest, or NULL on failure.
 * @note The caller is responsible for freeing the object with ParsedRequest_destroy().
 */
struct ParsedRequest* ParsedRequest_create();

/**
 * @brief Frees all memory associated with a ParsedRequest object.
 * @param pr The ParsedRequest object to destroy.
 */
void ParsedRequest_destroy(struct ParsedRequest *pr);

/**
 * @brief Parses a raw HTTP request string into a ParsedRequest structure.
 * @param pr The ParsedRequest object to populate.
 * @param buf The raw request string.
 * @param buflen The length of the request string.
 * @return 0 on success, -1 on failure.
 */
int ParsedRequest_parse(struct ParsedRequest *pr, const char *buf, size_t buflen);

/**
 * @brief Calculates the length of the unparsed request line.
 * @param pr The ParsedRequest object.
 * @return The length of the request line in bytes.
 */
size_t ParsedRequest_requestLineLen(struct ParsedRequest *pr);

/**
 * @brief Reconstructs the request line into a buffer.
 * @param pr The ParsedRequest object.
 * @param buf Buffer to write the unparsed request line into.
 * @param buflen Size of the provided buffer.
 * @return The number of bytes written on success, -1 on failure.
 */
int ParsedRequest_unparse(struct ParsedRequest *pr, char *buf, size_t buflen);

/**
 * @brief Reconstructs only the headers into a string.
 * @param pr The ParsedRequest object.
 * @param buf Buffer to write the unparsed headers into.
 * @param buflen Size of the provided buffer.
 * @return The number of bytes written on success, -1 on failure.
 */
int ParsedRequest_unparse_headers(struct ParsedRequest *pr, char *buf, size_t buflen);

/**
 * @brief Gets the total length of all headers if they were unparsed.
 * @param pr The ParsedRequest object.
 * @return The total length in bytes.
 */
size_t ParsedHeader_headersLen(struct ParsedRequest *pr);

/**
 * @brief Sets a header value, overwriting if the key already exists.
 * @param pr The ParsedRequest object.
 * @param key The header key (e.g., "Host").
 * @param value The header value.
 * @return 0 on success, -1 on failure.
 */
int ParsedHeader_set(struct ParsedRequest *pr, const char *key, const char *value);

/**
 * @brief Retrieves a header by its key.
 * @param pr The ParsedRequest object.
 * @param key The header key to find.
 * @return A pointer to the ParsedHeader struct if found, otherwise NULL.
 * @note The returned pointer is owned by the ParsedRequest object and should not be freed.
 */
struct ParsedHeader* ParsedHeader_get(struct ParsedRequest *pr, const char *key);

/**
 * @brief Removes a header by its key.
 * @param pr The ParsedRequest object.
 * @param key The header key to remove.
 * @return 0 on success, -1 if the key was not found.
 */
int ParsedHeader_remove(struct ParsedRequest *pr, const char *key);

#endif

