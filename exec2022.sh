# 1. COMPILAZIONE
# Si può compilare il file "normalmente" oppure in debug-mode. Quest'ultima stampa un maggior numero di informazioni durante l'esecuzione.

# Meccanismo per compilare correttamente con o senza le informazioni di debug. Vedi il makefile per maggiori informazioni sul problema.
# Il file "debug" è usato come discriminante per capire se la precedente compilazione contiene o meno la macro DEBUG.
# Il file esisterà SOLO se nella compilazione è stata inserita la macro DEBUG tramite il parametro -D di gcc (CPPFLAG).
# Volendo si può fare a meno del file "debug", ma così facendo saremmo costretti a chiamare ogni volta la 'make clean', che obbliga
# a ricompilare i file anche se non necessario, per esempio perché i file sorgente non sono stati modificati (viene eseguita di nuovo
# la compilazione perché i file vengono eliminati dalla 'make clean' e quindi il makefile eseguirà tutti i comandi nelle make rules).
# In sintesi, con l'uso del file "debug" si può sfruttare al massimo il makefile (compila solo quando i .c sono modificati), altrimenti no.
  if [[ $1 == "debug" ]]; then
    # Se è stata eseguita almeno una volta la compilazione (controllo il file 'dev' ma potrei controllarne anche altri) E
    # se il file 'debug' non esiste => la precedente compilazione non conteneva le informazioni di debug
    if [[ -f dev && ! -f debug ]]; then
      make clean # Rimuovo il file "debug" e i file compilati, così da poterci inserire la macro DEBUG
    fi
    touch debug # Creo il file "debug" (adesso la compilazione conterrà la macro DEBUG)
    make debug # Chiamo la make rule 'debug' che definisce la macro DEBUG
  else
    # Se è stata eseguita almeno una volta la compilazione (controllo il file 'dev' ma potrei controllarne anche altri) E
    # se il file 'debug' esiste => la precedente compilazione conteneva le informazioni di debug
    if [[ -f dev && -f debug ]]; then
      make clean # Rimuovo i file compilati contenenti la macro DEBUG e il file "debug"
    fi
    make
  fi
  read -p "Compilazione eseguita. Premi invio per eseguire..."


# 2. ESECUZIONE
# I file eseguibili di server e device devono
# chiamarsi 'serv' e 'dev', e devono essere nella current folder

# 2.1 esecuzioe del server sulla porta 4242
  gnome-terminal -x sh -c "./serv 4242; exec bash"

# 2.2 esecuzione di 3 device sulle porte {5001,...,5003}
  for port in {5001..5003}
  do
     gnome-terminal -x sh -c "./dev $port; exec bash"
  done


# RUBRICA:
# gli username degli utenti devono essere 'user1' 'user2' e 'user3'.
# Per semplicità, non deve essere gestita la rubrica di ogni utente.
# Fare in modo che le rubriche degli utenti siano le seguenti:
#  'user1' ha in rubrica 'user2'
#  'user2' ha in rubrica 'user1' e 'user3'
#  'user3' ha in rubrica 'user2'.