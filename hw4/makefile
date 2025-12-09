CC = gcc
CFLAGS = -O3 -fopenmp

# All source files
SRCS = baseline_3.c \
       cache_optimized_4.c \
       parallel_3.c \
       parallel_cache.c \
       seq_cache.c \
       seq_dram.c \
       seq_para.c \
       hw4.c

# Generate executable names from src
EXES = $(SRCS:.c=.out)

# ------------------------
# Default: build everything
# ------------------------
all: $(EXES)

# Pattern rule: build X.c → X.out
%.out: %.c
	$(CC) $(CFLAGS) -o $@ $<

# --------------------------------
# Run one file:
#    make run SRC=baseline_3.c FILE=data.bin
# --------------------------------
run:
	@if [ -z "$(SRC)" ] || [ -z "$(FILE)" ]; then \
		echo "Usage: make run SRC=xxx.c FILE=data.bin"; exit 1; \
	fi
	$(CC) $(CFLAGS) -o run_exec $(SRC)
	./run_exec $(FILE)

# --------------------------------
# Clean
# --------------------------------
clean:
	rm -f *.out run_exec