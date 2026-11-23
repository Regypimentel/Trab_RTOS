CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -pthread
LDLIBS  = -lrt -pthread
BINS    = elevador passageiros painel corrida

all: $(BINS)
%: src/%.c src/comum.h
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

run: all
	./elevador
clean:
	rm -f $(BINS)
