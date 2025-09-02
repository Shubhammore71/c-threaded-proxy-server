/**
 * @file proxy_server.c
 * @author Your Name
 * @brief A multi-threaded caching HTTP proxy server.
 *
 * This proxy server listens for incoming client connections, parses HTTP
 * requests, and forwards them to the destination server. It supports
 * multi-threading to handle multiple clients concurrently.
 *
 * Caching can be enabled by compiling with the -DENABLE_CACHE flag. When
 * enabled, it uses an LRU cache to store and serve responses for repeated
 * requests, reducing latency and network traffic.
 */

#include "proxy_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#ifdef ENABLE_CACHE
#include "cache.h"
#endif

// --- Constants ---
#define MAX_REQUEST_SIZE 8192      // Max size of an HTTP request
#define MAX_CLIENTS 100            // Max concurrent clients
#define DEFAULT_PORT 8080          // Default port to listen on

// --- Cache Configuration ---
#define MAX_CACHE_SIZE (200 * 1024 * 1024)    // 200 MB total cache size
#define MAX_ELEMENT_SIZE (10 * 1024 * 1024)   // 10 MB max size for a single cached item

// --- Structs ---

// Arguments for the connection handler thread
typedef struct {
    int client_socket_fd;
} thread_args_t;

// --- Function Prototypes ---
void *handle_connection(void *args);
void forward_request_and_get_response(int client_socket_fd, struct ParsedRequest *req, const char *url_string);
int send_error_to_client(int client_socket_fd, int status_code, const char *status_message);
void signal_handler(int signum);

// --- Main Function ---
int main(int argc, char *argv[]) {
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number. Using default %d.\n", DEFAULT_PORT);
        port = DEFAULT_PORT;
    }

    // --- Signal Handling Setup ---
    signal(SIGPIPE, SIG_IGN); // Ignore broken pipe signals
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL); // Handle Ctrl+C

    // --- Cache Initialization ---
    #ifdef ENABLE_CACHE
    if (cache_init(MAX_CACHE_SIZE, MAX_ELEMENT_SIZE) != 0) {
        fprintf(stderr, "Failed to initialize cache. Exiting.\n");
        exit(EXIT_FAILURE);
    }
    printf("Cache enabled.\n");
    #else
    printf("Cache disabled.\n");
    #endif

    // --- Socket Setup ---
    int server_socket_fd;
    struct sockaddr_in server_addr;
    
    server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    
    // Allow address reuse
    int optval = 1;
    setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_socket_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        close(server_socket_fd);
        exit(EXIT_FAILURE);
    }

    printf("Proxy server listening on port %d...\n", port);

    // --- Main Accept Loop ---
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket_fd = accept(server_socket_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_socket_fd < 0) {
            if (errno == EINTR) continue; // Interrupted by a signal, continue loop
            perror("accept");
            continue;
        }
        
        // Prepare arguments for the new thread
        thread_args_t *args = malloc(sizeof(thread_args_t));
        if (!args) {
            perror("malloc for thread args");
            close(client_socket_fd);
            continue;
        }
        args->client_socket_fd = client_socket_fd;
        
        // Create a new thread to handle the connection
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_connection, (void *)args) != 0) {
            perror("pthread_create");
            free(args);
            close(client_socket_fd);
        }
        
        // Detach the thread so its resources are automatically released
        pthread_detach(thread_id);
    }

    // Cleanup (should not be reached in normal operation)
    close(server_socket_fd);
    #ifdef ENABLE_CACHE
    cache_destroy();
    #endif

    return 0;
}


/**
 * @brief Handles a single client connection from start to finish.
 * This function is the entry point for each new thread.
 */
void *handle_connection(void *args) {
    thread_args_t *thread_args = (thread_args_t *)args;
    int client_socket_fd = thread_args->client_socket_fd;
    free(thread_args);

    char request_buffer[MAX_REQUEST_SIZE];
    
    // Read the client's request
    ssize_t bytes_read = recv(client_socket_fd, request_buffer, sizeof(request_buffer) - 1, 0);
    if (bytes_read <= 0) {
        if (bytes_read < 0) perror("recv");
        close(client_socket_fd);
        return NULL;
    }
    request_buffer[bytes_read] = '\0';

    // Parse the request
    struct ParsedRequest *req = ParsedRequest_create();
    if (ParsedRequest_parse(req, request_buffer, bytes_read) < 0) {
        fprintf(stderr, "Failed to parse request.\n");
        send_error_to_client(client_socket_fd, 400, "Bad Request");
        ParsedRequest_destroy(req);
        close(client_socket_fd);
        return NULL;
    }
    
    // Ensure we have a host to connect to
    if (!req->host) {
        fprintf(stderr, "Request is missing Host header or absolute URI.\n");
        send_error_to_client(client_socket_fd, 400, "Bad Request");
        ParsedRequest_destroy(req);
        close(client_socket_fd);
        return NULL;
    }

    // Form the full URL to use as the cache key
    char url_string[MAX_REQUEST_SIZE];
    snprintf(url_string, sizeof(url_string), "%s://%s:%s%s", req->protocol, req->host, req->port, req->path);

    printf("Received request for: %s\n", url_string);

    #ifdef ENABLE_CACHE
    int cache_data_size = 0;
    char *cached_data = cache_get(url_string, &cache_data_size);

    if (cached_data) {
        printf("Cache HIT for: %s\n", url_string);
        // Send cached data back to the client
        send(client_socket_fd, cached_data, cache_data_size, 0);
        free(cached_data);
    } else {
        printf("Cache MISS for: %s\n", url_string);
        // Forward request to origin server
        forward_request_and_get_response(client_socket_fd, req, url_string);
    }
    #else
    // Without cache, always forward the request
    forward_request_and_get_response(client_socket_fd, req, url_string);
    #endif

    ParsedRequest_destroy(req);
    close(client_socket_fd);
    return NULL;
}


/**
 * @brief Connects to the destination server, forwards the request, reads the
 * response, and relays it back to the client.
 */
void forward_request_and_get_response(int client_socket_fd, struct ParsedRequest *req, const char *url_string) {
    // --- Connect to Destination Server ---
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(req->host, req->port, &hints, &res) != 0) {
        perror("getaddrinfo");
        send_error_to_client(client_socket_fd, 502, "Bad Gateway");
        return;
    }

    int dest_socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (dest_socket_fd < 0) {
        perror("socket (destination)");
        freeaddrinfo(res);
        send_error_to_client(client_socket_fd, 502, "Bad Gateway");
        return;
    }

    if (connect(dest_socket_fd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect (destination)");
        freeaddrinfo(res);
        close(dest_socket_fd);
        send_error_to_client(client_socket_fd, 502, "Bad Gateway");
        return;
    }
    freeaddrinfo(res);
    
    // --- Forward Request ---
    // Modify request for proxying (HTTP/1.0, remove proxy-specific headers)
    ParsedHeader_set(req, "Host", req->host);
    ParsedHeader_set(req, "Connection", "close");
    req->version = "HTTP/1.0"; // Use HTTP/1.0 for simplicity with origin servers

    size_t req_line_len = ParsedRequest_requestLineLen(req);
    size_t headers_len = ParsedHeader_headersLen(req);
    size_t total_req_len = req_line_len + headers_len;

    char *request_to_send = malloc(total_req_len + 1);
    if (!request_to_send) {
        close(dest_socket_fd);
        send_error_to_client(client_socket_fd, 500, "Internal Server Error");
        return;
    }

    int written_line_len = ParsedRequest_unparse(req, request_to_send, total_req_len);
    if (written_line_len < 0) {
        fprintf(stderr, "Failed to unparse request line.\n");
        free(request_to_send);
        close(dest_socket_fd);
        return;
    }

    ParsedRequest_unparse_headers(req, request_to_send + written_line_len, total_req_len - written_line_len);
    
    if (send(dest_socket_fd, request_to_send, total_req_len, 0) < 0) {
        perror("send (destination)");
        free(request_to_send);
        close(dest_socket_fd);
        return;
    }
    free(request_to_send);

    // --- Relay Response to Client ---
    char response_buffer[MAX_REQUEST_SIZE];
    ssize_t bytes_read;

    #ifdef ENABLE_CACHE
    // Buffer to hold the entire response for caching
    char *full_response = NULL;
    size_t total_response_size = 0;
    size_t current_buffer_capacity = 0;
    #endif

    while ((bytes_read = recv(dest_socket_fd, response_buffer, sizeof(response_buffer), 0)) > 0) {
        // Send chunk to client immediately
        if (send(client_socket_fd, response_buffer, bytes_read, 0) < 0) {
            perror("send (client)");
            break;
        }

        #ifdef ENABLE_CACHE
        // Append chunk to our full response buffer for caching
        if (total_response_size + bytes_read > current_buffer_capacity) {
             current_buffer_capacity = (total_response_size + bytes_read) * 2;
             char *new_buffer = realloc(full_response, current_buffer_capacity);
             if (!new_buffer) {
                 free(full_response);
                 full_response = NULL; // Stop trying to cache
             } else {
                 full_response = new_buffer;
             }
        }
        if (full_response) {
            memcpy(full_response + total_response_size, response_buffer, bytes_read);
            total_response_size += bytes_read;
        }
        #endif
    }
    
    if (bytes_read < 0) {
        perror("recv (destination)");
    }

    #ifdef ENABLE_CACHE
    if (full_response && total_response_size > 0) {
        cache_put(url_string, full_response, total_response_size);
        free(full_response);
    }
    #endif

    close(dest_socket_fd);
}

/**
 * @brief Sends a simple HTTP error response to the client.
 */
int send_error_to_client(int client_socket_fd, int status_code, const char *status_message) {
    char response[512];
    int len = snprintf(response, sizeof(response),
        "HTTP/1.0 %d %s\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n",
        status_code, status_message);
    
    return send(client_socket_fd, response, len, 0);
}


/**
 * @brief Handles signals for graceful shutdown.
 */
void signal_handler(int signum) {
    if (signum == SIGINT) {
        printf("\nCaught SIGINT. Shutting down gracefully...\n");
        #ifdef ENABLE_CACHE
        cache_destroy();
        printf("Cache cleaned up.\n");
        #endif
        exit(0);
    }
}

