CC      = gcc
CFLAGS  = -O2 -Wall
LDFLAGS = -lnuma -lpthread

TARGETS = phys_try phys_bench_single phys_bench_multi phys_bench_prefetch phys_dma_sim phys_peek phys_debug_dmawrite phys_debug_dmaread

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

phys_debug_dmawrite: phys_debug_dmawrite.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

phys_debug_dmaread: phys_debug_dmaread.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS)

.PHONY: all clean

