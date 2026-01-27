CC      = gcc
CFLAGS  = -O2 -Wall
LDFLAGS = -lnuma -lpthread

TARGETS = phys_try phys_bench_single phys_bench_multi phys_bench_prefetch phys_dma_sim phys_peek

all: $(TARGETS)

phys_try: phys_try.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

phys_bench_single: phys_bench_single.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

phys_bench_multi: phys_bench_multi.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

phys_bench_prefetch: phys_bench_prefetch.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

phys_dma_sim: phys_dma_sim.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

phys_peek: phys_peek.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS)

.PHONY: all clean

