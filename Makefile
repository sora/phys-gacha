CC      = gcc
CFLAGS  = -O2 -Wall
LDFLAGS = -lnuma -lpthread

TARGETS = phys_try phys_bench phys_dma_sim phys_peek

all: $(TARGETS)

phys_try: phys_try.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

phys_bench: phys_bench.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

phys_dma_sim: phys_dma_sim.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

phys_peek: phys_peek.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS)

.PHONY: all clean

