CC = gcc
CFLAGS = -O3 -fopenmp

# List of all source files
SRCS = baseline_3.c \
       cache_optimized_4.c \
       parallel_3.c \
       parallel_cache.c \
       seq_cache.c \
       seq_dram.c \
       seq_para.c \
       hw4.c

# Executables
EXES = $(SRCS:.c=.out)

# ------------------------------------------------------
# Build all executables
# ------------------------------------------------------
all: $(EXES)

%.out: %.c
	$(CC) $(CFLAGS) -o $@ $<

# ------------------------------------------------------
# Run one manually:
#   make run SRC=seq_dram.c FILE=data.bin
# ------------------------------------------------------
run:
	@if [ -z "$(SRC)" ] || [ -z "$(FILE)" ]; then \
		echo "Usage: make run SRC=xxx.c FILE=data.bin"; exit 1; \
	fi
	$(CC) $(CFLAGS) -o run_exec $(SRC)
	./run_exec $(FILE)

# ------------------------------------------------------
# Auto test all .out executables and generate results.csv
# Usage:
#   make test FILE=data.bin
# ------------------------------------------------------
test:
	@if [ -z "$(FILE)" ]; then \
		echo "Usage: make test FILE=data.bin"; exit 1; \
	fi; \
	\
	echo "name,time_ms,memory_mb" > results.csv; \
	\
	for exe in $(EXES); do \
		echo "======================================="; \
		echo "Running $$exe ..."; \
		OUTPUT=$$(./$$exe $(FILE) | tee /dev/tty); \
		TIME=$$(echo "$$OUTPUT" | grep -i "Time" | awk '{print $$NF}'); \
		MEM=$$(echo "$$OUTPUT"  | grep -i "RSS"  | awk '{print $NF}'); \
		NAME=$${exe%.out}; \
		\
		echo "$$NAME,$$TIME,$$MEM" >> results.csv; \
	done; \
	\
	echo "======================================="; \
	echo "Results saved into results.csv";

# ------------------------------------------------------
# Clean
# ------------------------------------------------------
clean:
	rm -f *.out run_exec results.csv