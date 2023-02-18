# make rule primaria
all: device server

# make rule che aggiunge le informazioni di debug
# PROBLEMA: se eseguiamo 'make' (quindi senza le informazioni di debug) e poi lanciamo 'make debug'
# (che imposta la macro DEBUG) o viceversa, non verrà fatto niente poiché i file sorgente non sono stati modificati,
# dunque non verrà eseguito il comando di compilazione (gcc) con o senza il parametro (-D) che imposta la macro DEBUG;
# ergo non verrà definita/rimossa la macro DEBUG e il programma rimarrà/non entrerà nella debugging-mode (con molte più stampe).
# SOLUZIONE 1: usare 'make clean' tra i due tipi di compilazione diversi (esempio: 'make debug', 'make clean', 'make').
# SOLUZIONE 2 (raccomandata): usare lo script './exec2022.sh debug' che nasconde tutti i passaggi sopra riportati.
debug: all
debug: DEBUG=-DDEBUG # parametro di gcc per settare la macro DEBUG (CPPFLAG)


# make rule per i device
device: device.o costanti.h util/messaggi.o util/string.o util/file.o util/time.o
	gcc -Wall device.o util/messaggi.o util/string.o util/file.o util/time.o -o dev

device.o: device.c
	gcc -Wall $(DEBUG) -c device.c


# make rule per il server
server: server.o struct/registro.h costanti.h util/messaggi.o util/string.o util/file.o util/time.o
	gcc -Wall server.o util/messaggi.o util/string.o util/file.o util/time.o -o serv

server.o: server.c
	gcc -Wall $(DEBUG) -c server.c


# make rule per i sorgenti di utility
util/messaggi.o: util/messaggi.c util/messaggi.h util/string.o
	gcc -Wall $(DEBUG) -c util/messaggi.c -o $@

util/string.o: util/string.c util/string.h
	gcc -Wall $(DEBUG) -c util/string.c -o $@

util/file.o: util/file.c util/file.h costanti.h
	gcc -Wall $(DEBUG) -c util/file.c -o $@

util/time.o: util/time.c util/time.h costanti.h
	gcc -Wall $(DEBUG) -c util/time.c -o $@


# pulizia dei file della compilazione
clean:
	rm -f *o struct/*o util/*o debug dev serv