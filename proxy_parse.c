/**
 * @file proxy_parse.c
 * @author Your Name
 * @brief Implementation of a simple HTTP request parser.
 *
 * This file provides the logic for parsing HTTP requests as defined in
 * proxy_parse.h. It handles memory management, string manipulation, and
 * header processing.
 *
 * Based on the original proxy_parse.c.
 */

#include "proxy_parse.h"

#define DEFAULT_NHDRS 8
#define MAX_REQ_LEN 65535
#define MIN_REQ_LEN 4

// Private function declarations
static int ParsedRequest_printRequestLine(struct ParsedRequest *pr, char *buf, size_t buflen, size_t *tmp);

/**
 * @brief Prints debugging information to stderr if DEBUG is enabled.
 */
void debug(const char *format, ...) {
    va_list args;
    if (DEBUG) {
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
}

/**
 * @brief Creates a new ParsedRequest object.
 */
struct ParsedRequest* ParsedRequest_create() {
    struct ParsedRequest *pr = (struct ParsedRequest *) malloc(sizeof(struct ParsedRequest));
    if (pr != NULL) {
        pr->method = NULL;
        pr->protocol = NULL;
        pr->host = NULL;
        pr->port = NULL;
        pr->path = NULL;
        pr->version = NULL;
        pr->buf = NULL;
        pr->buflen = 0;
        pr->headers = (struct ParsedHeader *) malloc(sizeof(struct ParsedHeader) * DEFAULT_NHDRS);
        if (pr->headers == NULL) {
            free(pr);
            return NULL;
        }
        pr->headers_used = 0;
        pr->headers_len = DEFAULT_NHDRS;
    }
    return pr;
}

/**
 * @brief Destroys a ParsedRequest object, freeing all associated memory.
 */
void ParsedRequest_destroy(struct ParsedRequest *pr) {
    if (pr == NULL) return;
    if (pr->buf != NULL) free(pr->buf);

    for (size_t i = 0; i < pr->headers_used; i++) {
        free(pr->headers[i].key);
        free(pr->headers[i].value);
    }
    free(pr->headers);
    free(pr);
}

/**
 * @brief Parses the request line of an HTTP request.
 * @return 0 on success, -1 on failure.
 */
static int parse_request_line(struct ParsedRequest *pr, const char *request_line) {
    char *method, *uri, *version;
    char *uri_ptr;
    char *host_port;
    char *port_ptr;

    // Method
    method = strstr(request_line, " ");
    if (method == NULL) return -1;
    pr->method = strndup(request_line, method - request_line);
    
    // URI
    uri = method + 1;
    version = strstr(uri, " ");
    if (version == NULL) { free(pr->method); pr->method = NULL; return -1; }
    pr->version = strndup(version + 1, strlen(version + 1));

    uri_ptr = strndup(uri, version - uri);

    // Protocol
    if (strstr(uri_ptr, "://") != NULL) {
        pr->protocol = strndup(uri_ptr, strstr(uri_ptr, "://") - uri_ptr);
        host_port = strstr(uri_ptr, "://") + 3;
    } else {
        pr->protocol = strdup("http");
        host_port = uri_ptr;
    }

    // Path
    pr->path = strstr(host_port, "/");
    if (pr->path != NULL) {
        pr->path = strdup(pr->path);
    } else {
        pr->path = strdup("/");
    }
    
    // Host and Port
    host_port = strndup(host_port, strlen(host_port) - strlen(pr->path));
    port_ptr = strstr(host_port, ":");
    if (port_ptr != NULL) {
        pr->port = strdup(port_ptr + 1);
        pr->host = strndup(host_port, port_ptr - host_port);
    } else {
        pr->port = strdup("80");
        pr->host = strdup(host_port);
    }

    free(uri_ptr);
    free(host_port);

    return 0;
}


/**
 * @brief Main parsing function for an entire HTTP request buffer.
 */
int ParsedRequest_parse(struct ParsedRequest *pr, const char *buf, size_t buflen) {
    if (pr == NULL || buf == NULL || buflen < MIN_REQ_LEN) return -1;

    // Create a mutable copy of the buffer
    pr->buf = (char *) malloc(buflen + 1);
    if (pr->buf == NULL) return -1;
    memcpy(pr->buf, buf, buflen);
    pr->buf[buflen] = '\0';
    pr->buflen = buflen;

    char *request_line_end = strstr(pr->buf, "\r\n");
    if (request_line_end == NULL) {
        free(pr->buf);
        pr->buf = NULL;
        return -1;
    }

    *request_line_end = '\0'; // Null-terminate the request line
    if (parse_request_line(pr, pr->buf) < 0) {
        free(pr->buf);
        pr->buf = NULL;
        return -1;
    }
    *request_line_end = '\r'; // Restore for header parsing

    // Parse headers
    char *current_header_line = request_line_end + 2;
    while (current_header_line < pr->buf + pr->buflen && *current_header_line != '\r') {
        char *header_end = strstr(current_header_line, "\r\n");
        if (header_end == NULL) break;
        
        char *colon = strstr(current_header_line, ":");
        if (colon == NULL || colon > header_end) {
            current_header_line = header_end + 2;
            continue;
        }

        char *key = strndup(current_header_line, colon - current_header_line);
        char *value = colon + 1;
        while (*value == ' ') value++; // Skip leading spaces in value
        value = strndup(value, header_end - value);

        ParsedHeader_set(pr, key, value);
        free(key);
        free(value);

        current_header_line = header_end + 2;
    }

    return 0;
}

/**
 * @brief Reconstructs the request line into a buffer.
 */
int ParsedRequest_unparse(struct ParsedRequest *pr, char *buf, size_t buflen) {
    if (pr == NULL || buf == NULL) return -1;

    size_t tmp;
    if (ParsedRequest_printRequestLine(pr, buf, buflen, &tmp) < 0) return -1;

    return (int)tmp;
}

/**
 * @brief Reconstructs only the headers into a string.
 */
int ParsedRequest_unparse_headers(struct ParsedRequest *pr, char *buf, size_t buflen) {
    if (!pr || !buf) return -1;

    char *current = buf;
    size_t tmp = 0;

    for (size_t i = 0; i < pr->headers_used; i++) {
        const char *key = pr->headers[i].key;
        const char *value = pr->headers[i].value;
        size_t keylen = strlen(key);
        size_t valuelen = strlen(value);
        
        if (tmp + keylen + valuelen + 4 > buflen) {
            debug("Buffer too small for headers.\n");
            return -1;
        }

        memcpy(current, key, keylen);
        current += keylen;
        memcpy(current, ": ", 2);
        current += 2;
        memcpy(current, value, valuelen);
        current += valuelen;
        memcpy(current, "\r\n", 2);
        current += 2;
        tmp += keylen + valuelen + 4;
    }

    // Add final CRLF for header section end
    if (tmp + 2 > buflen) {
        debug("Buffer too small for final CRLF.\n");
        return -1;
    }
    memcpy(current, "\r\n", 2);
    tmp += 2;

    return (int)tmp;
}

/**
 * @brief Gets the total length of all headers if they were unparsed.
 */
size_t ParsedHeader_headersLen(struct ParsedRequest *pr) {
    if (!pr) return 0;
    size_t len = 0;
    for (size_t i = 0; i < pr->headers_used; i++) {
        len += strlen(pr->headers[i].key) + strlen(pr->headers[i].value) + 4; // ": " and "\r\n"
    }
    len += 2; // Final "\r\n"
    return len;
}

/**
 * @brief Sets a header value, overwriting if the key already exists.
 */
int ParsedHeader_set(struct ParsedRequest *pr, const char *key, const char *value) {
    if (!pr || !key || !value) return -1;

    // First, try to find and update existing header
    for (size_t i = 0; i < pr->headers_used; i++) {
        if (strcasecmp(pr->headers[i].key, key) == 0) {
            free(pr->headers[i].value);
            pr->headers[i].value = strdup(value);
            return pr->headers[i].value == NULL ? -1 : 0;
        }
    }

    // If not found, add a new one
    if (pr->headers_used >= pr->headers_len) {
        pr->headers_len *= 2;
        struct ParsedHeader *new_headers = (struct ParsedHeader *) realloc(pr->headers, pr->headers_len * sizeof(struct ParsedHeader));
        if (new_headers == NULL) {
            pr->headers_len /= 2; // Revert on failure
            return -1;
        }
        pr->headers = new_headers;
    }

    pr->headers[pr->headers_used].key = strdup(key);
    pr->headers[pr->headers_used].value = strdup(value);

    if (pr->headers[pr->headers_used].key == NULL || pr->headers[pr->headers_used].value == NULL) {
        // Cleanup on allocation failure
        free(pr->headers[pr->headers_used].key);
        free(pr->headers[pr->headers_used].value);
        return -1;
    }
    
    pr->headers_used++;
    return 0;
}

/**
 * @brief Retrieves a header by its key.
 */
struct ParsedHeader* ParsedHeader_get(struct ParsedRequest *pr, const char *key) {
    if (!pr || !key) return NULL;
    for (size_t i = 0; i < pr->headers_used; i++) {
        if (strcasecmp(pr->headers[i].key, key) == 0) {
            return &pr->headers[i];
        }
    }
    return NULL;
}

/**
 * @brief Removes a header by its key.
 */
int ParsedHeader_remove(struct ParsedRequest *pr, const char *key) {
    if (!pr || !key) return -1;
    for (size_t i = 0; i < pr->headers_used; i++) {
        if (strcasecmp(pr->headers[i].key, key) == 0) {
            free(pr->headers[i].key);
            free(pr->headers[i].value);
            // Shift remaining headers left
            if (i < pr->headers_used - 1) {
                memmove(&pr->headers[i], &pr->headers[i + 1], (pr->headers_used - 1 - i) * sizeof(struct ParsedHeader));
            }
            pr->headers_used--;
            return 0;
        }
    }
    return -1; // Not found
}

/**
 * @brief Calculates the length of the request line.
 */
size_t ParsedRequest_requestLineLen(struct ParsedRequest *pr) {
    if (!pr || !pr->method || !pr->path || !pr->version) return 0;
    // Format: "METHOD PATH VERSION\r\n"
    return strlen(pr->method) + 1 + strlen(pr->path) + 1 + strlen(pr->version) + 2;
}

/**
 * @brief Writes the request line to a buffer. (Private)
 */
static int ParsedRequest_printRequestLine(struct ParsedRequest *pr, char *buf, size_t buflen, size_t *tmp) {
    if (!pr || !buf) return -1;
    char *current = buf;
    
    if (buflen < ParsedRequest_requestLineLen(pr)) {
        debug("Buffer not large enough for request line.\n");
        return -1;
    }
    
    memcpy(current, pr->method, strlen(pr->method));
    current += strlen(pr->method);
    *current++ = ' ';
    
    memcpy(current, pr->path, strlen(pr->path));
    current += strlen(pr->path);
    *current++ = ' ';
    
    memcpy(current, pr->version, strlen(pr->version));
    current += strlen(pr->version);
    
    *tmp = (size_t)(current - buf);
    return 0;
}

