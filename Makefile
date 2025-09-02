# Makefile for the Multi-Threaded Proxy Server

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
LDFLAGS = -pthread

# Executable names
TARGET = proxy_server
TARGET_WITH_CACHE = proxy_server_with_cache

# --- Targets ---

# The 'all' target is the default. It builds both versions.
all: $(TARGET) $(TARGET_WITH_CACHE)

# Target: proxy_server (without cache)
# This rule compiles and links all necessary sources.
$(TARGET): proxy_server.c proxy_parse.c
	$(CC) $(CFLAGS) -o $(TARGET) proxy_server.c proxy_parse.c $(LDFLAGS)
	@echo "---"
	@echo "Created proxy_server (caching disabled)"
	@echo "To run: ./proxy_server [port]"
	@echo "---"


# Target: proxy_server_with_cache
# This rule compiles and links all necessary sources for the caching version.
$(TARGET_WITH_CACHE): proxy_server.c proxy_parse.c cache.c
	$(CC) $(CFLAGS) -DENABLE_CACHE -o $(TARGET_WITH_CACHE) proxy_server.c proxy_parse.c cache.c $(LDFLAGS)
	@echo "---"
	@echo "Created proxy_server_with_cache (caching enabled)"
	@echo "To run: ./proxy_server_with_cache [port]"
	@echo "---"

# Phony targets are not files. 'clean' is a common phony target.
.PHONY: all clean

# Clean target to remove generated files
clean:
	rm -f $(TARGET) $(TARGET_WITH_CACHE) *.o
	@echo "Cleaned up project files."
