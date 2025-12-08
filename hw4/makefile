# ======================================================
# Makefile for merged_sort_framework.c
# A = concurrency
# B = cache-block
# C = sequential DRAM
# D = baseline
#
# macOS: use Homebrew gcc-15 (supports OpenMP)
# Linux: use system gcc (supports OpenMP)
# ======================================================

TARGET = sort
SRC = hw4.c
CC = gcc
CFLAGS = -O3 -fopenmp

# ------------------------------------------------------
# Build
# ------------------------------------------------------
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

# ------------------------------------------------------
# Run with a file (default baseline)
# Usage: make run FILE=data.bin
# ------------------------------------------------------
run: 
	@if [ -z "$(FILE)" ]; then \
		echo "Usage: make run FILE=data.bin"; exit 1; \
	fi
	$(CC) $(CFLAGS) -DUSE_PARALLEL=0 -DUSE_CACHE_BLOCK=0 -DUSE_SEQ_DRAM=0 -DUSE_BASELINE=1 \
	    -o $(TARGET) $(SRC)
	./$(TARGET) $(FILE)

# ------------------------------------------------------
# Run ONE combination manually
# Usage:
#   make run_one A=1 B=0 C=1 D=0 FILE=data.bin
# ------------------------------------------------------
run_one:
	@if [ -z "$(FILE)" ]; then \
		echo "Usage: make run_one A=? B=? C=? D=? FILE=data.bin"; exit 1; \
	fi
	$(CC) $(CFLAGS) -DUSE_PARALLEL=$(A) -DUSE_CACHE_BLOCK=$(B) -DUSE_SEQ_DRAM=$(C) -DUSE_BASELINE=$(D) \
	    -o $(TARGET) $(SRC)
	./$(TARGET) $(FILE)

# ------------------------------------------------------
# Run ALL 8 meaningful combinations
# Usage: 
#   make run_all FILE=data.bin
# ------------------------------------------------------
run_all:
	@if [ -z "$(FILE)" ]; then \
		echo "Usage: make run_all FILE=data.bin"; exit 1; \
	fi; \
	\
	echo "Name,A,B,C,D,Time_sec,Memory_MB" > results.csv; \
	\
	combos=" \
0 0 0 1 baseline \
1 0 0 0 parallel \
0 1 0 0 cacheblock \
0 0 1 0 seqdram \
1 1 0 0 ab \
1 0 1 0 ac \
0 1 1 0 bc \
1 1 1 0 abc \
"; \
	\
	for c in $$combos; do \
		set -- $$c; \
		A=$$1; B=$$2; C=$$3; D=$$4; NAME=$$5; \
		echo "===================================================="; \
		echo "Running: $$NAME  (A=$$A B=$$B C=$$C D=$$D)"; \
		\
		$(CC) $(CFLAGS) -DUSE_PARALLEL=$$A -DUSE_CACHE_BLOCK=$$B -DUSE_SEQ_DRAM=$$C -DUSE_BASELINE=$$D \
			-o $(TARGET) $(SRC); \
		\
		OUTPUT=$$(./$(TARGET) $(FILE) | tee /dev/tty); \
		TIME=$$(echo "$$OUTPUT" | grep "Time:" | awk '{print $$2}'); \
		MEM=$$(echo "$$OUTPUT" | grep "Memory" | awk '{print $$4}'); \
		\
		echo "$$NAME,$$A,$$B,$$C,$$D,$$TIME,$$MEM" >> results.csv; \
	done; \
	\
	echo "===================================================="; \
	echo "Results saved to results.csv";

# ------------------------------------------------------
# Clean
# ------------------------------------------------------
clean:
	rm -f $(TARGET) results.csv