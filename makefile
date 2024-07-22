# definizione del compilatore e dei flag di compilazione
# che vengono usate dalle regole implicite
CC=gcc
CFLAGS=-std=c11 -Wall -g -O3 -pthread
LDLIBS=-lm -lrt -pthread

# elenco degli eseguibili da creare
EXECS=pagerank

# primo target: gli eseguibili sono precondizioni
# quindi verranno tutti creati
all: $(EXECS) 

# regola per la creazione degli eseguibili utilizzando xerrori.o
pagerank: pagerank.o xerrori.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# regola per la creazione di file oggetto che dipendono da xerrori.h
xerrori.o: xerrori.c xerrori.h
	$(CC) $(CFLAGS) -c $<

 
# esempio di target che non corrisponde a una compilazione
# ma esegue la cancellazione dei file oggetto e degli eseguibili
clean: 
	rm -f *.o *.log $(EXECS)
	
# crea file zip della lezione	
zip:
	zip Grossi_pagerank.zip *.c *.h *.py *.mtx makefile

