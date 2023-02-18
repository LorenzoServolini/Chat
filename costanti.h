/********************************
 *           GENERALE           *
*********************************/
#define DEFAULT_SERVER_PORT 4242 // Porta di default del server
#define QUEUE_LEN 10 // Massimo numero di client
#define INVALID_SOCKET (-1) // Permette di riconoscere un client disconnesso
#define READ_MARK "(**)" // Contrassegna i messaggi letti dal destinatario
#define UNREAD_MARK "(*)" // Contrassegna i messaggi non letti dal destinatario

/********************************
 *      DIMENSIONI MASSIME      *
*********************************/
#define USERNAME_LEN 30 // Lunghezza massima di un username
#define PASSWORD_LEN 60 // Lunghezza massima di una password
#define GROUP_SIZE 100 // Numero massimo di utenti in un gruppo
#define CONTACT_LIST_SIZE 200 // Numero massimo di contatti in rubrica
#define MAX_COMMAND_LEN (50 + USERNAME_LEN + PASSWORD_LEN) // Lunghezza massima di un comando inseribile da terminale
#define TIMESTAMP_LEN 50 // Lunghezza massima di un timestamp formattato
#define MAX_LINE_LEN MAX_COMMAND_LEN // Lunghezza massima di una riga in un file
#define FILE_MSG_SIZE 1023 // Quando si vuole condividere un file si inviano FILE_MSG_SIZE byte alla volta
#define MAX_MSG_LEN FILE_MSG_SIZE // Lunghezza massima di un messaggio (scambiato tra peer o tra client e server)

/********************************
 *             FILE             *
*********************************/
#define USERS_FILE "./users.txt" // File contenente le credenziali degli utenti
#define ACTIVITY_LOG_FILE "./activity.txt" // File su cui vengono registrati i login/logout degli utenti
#define OFFLINE_MSG_FILE "./messaggi_offline.txt" // File su cui vengono memorizzati i messaggi recapitati a utenti offline
#define SHARED_FILE_FOLDER "./shared/" // Cartella contenente i file che gli utenti possono condividere
#define CONTACT_LIST_FOLDER "./rubriche/" // Cartella contenente le rubriche di tutti gli utenti
#define CHAT_LOG_FOLDER "./chat/" // Cartella contenente i log delle chat tra ogni coppia di utenti
#define SHOW_LOG_FILE "./show_log.txt" // File di log contenente l'elenco delle show da notificare

/********************************
 *    COMANDI CLIENT<->SERVER   *
*********************************/
/*
 * 1) Il client invia il tipo di autenticazione (login o registrazione) al server
 * 2) Il client invia l'username al server
 * 3) Il client invia la password al server
 * 4) Il client invia la porta su cui è in ascolto
 * 5) Il server risponde con l'esito dell'operazione
 * 6) Il server notifica a tutti i peer che un utente è diventato online inviando il comando e poi il suo username
 */
#define SIGNUP "SGN" // Inviata al server per indicare che l'utente vuole registrarsi
#define LOGIN "LGN" // Inviata al server per indicare che l'utente vuole effettuare il login
#define ALREADY_EXISTING_USERNAME "OLDUSR" // Indica che l'username esiste già
#define SIGNED_UP "OKSGN" // Indica che la registrazione è avvenuta con successo
#define UNKNOWN_USER "UNKUSR" // Indica che l'username non esiste
#define WRONG_PASSWORD "WRGPSW" // Indica che la password non è quella corretta (per autenticarsi)
#define AUTHENTICATED "AUTHOK" // Indica che le credenziali sono corrette e l'autenticazione è avvenuta con successo
#define NOW_ONLINE "NEWONL" // Inviato dal server a tutti i peer per notificare il login di un utente

/*
 * 1) Si invia al server il comando di disconnessione
 */
#define LOGOUT_COMMAND "OUT" // Inviato al server per segnalare la disconnessione (e terminazione) del client

/*
 * 1) Viene inviato il comando che segnala l'intenzione di inviare un file
 * 2) I peer che devono ricevere il file inviano l'ACK (notifica la ricezione del comando (1))
 * 3) Inizia effettivamente l'invio del file
 * 4) Si notifica la fine del file
 */
#define SHARING_FILE "SHARE" // Mandato dal mittente per segnalare l'invio di un file condiviso
#define ACK_SHARE "OKSHARE" // Mandato dal ricevente per segnalare la ricezione del comando di condivisione file
#define DONE_SHARE "ENDSHARE" // Inviato dal mittente per segnalare la fine del file

/*
 * 1) Si invia al server il comando che segnala la volontà di iniziare una chat
 * 2) Si invia al server l'username dell'utente con cui si vuole avviare una chat
 * 3) Il server ci dice se l'utente è online o meno
 */
#define NEW_CHAT_COMMAND "CHT" // Inviato al server quando l'utente vuole avere le informazioni sui messaggi pendenti

/*
 * 1) Si invia al server il comando che segnala la volontà di avviare una chat di gruppo
 * 2) Si chiede al server se ogni utente in rubrica è online (si può aggiungere ad una chat solo utenti online)
 * 3) Il server controlla se l'utente è online e invia la risposta
 * 4) Si invia al server il comando che segnala la fine delle richieste per verificare se un utente è online o meno
 * 5) Si invia al server il comando per richiedere la porta di ascolto del client dell'utente da aggiungere
 * 6) Si invia l'username dell'utente di cui si vuole conoscere la porta di ascolto
 * 7) Il server informa se l'utente è sempre online (o se è andato offline nel frattempo)
 * 8) Se l'utente è ancora online, il server invia la porta di ascolto del client dell'utente
 * 9) Si invia all'utente che si vuole aggiungere l'invito per partecipare alla chat di gruppo
 * 10) Si invia all'utente l'username dell'utente che vuole inserirlo nella chat di gruppo (che gli ha mandato l'invito)
 * 11) L'utente invitato risponde
 * 12) L'utente invitato riceve gli username di tutti i membri della chat
 * 13) L'utente invitato riceve il segnale di fine username
 * 14) L'utente invitato richiede al server la porta di ascolto di tutti i membri della chat
 * 15) L'utente invitato invia al server l'username dell'utente di cui vuole conoscere la porta di ascolto
 * 16) Il server fornisce la porta del membro
 * 17) L'utente invitato invia ad ogni membro il comando per segnalare la sua aggiunta alla chat di gruppo e il suo username
 */
#define START_GROUP_CHAT "GRPCHAT" // Inviato dall'utente al server che vuole avviare una chat di gruppo
#define USER_ONLINE "ON" // Inviato dal server al richiedente, indica che l'utente richiesto è online
#define USER_OFFLINE "OFF" // Inviato dal server al richiedente, indica che l'utente richiesto è offline
#define GROUP_CHAT_DONE "GRPDONE" // Inviato al server per segnalare la fine delle richieste per verificare se un utente è online
#define CLIENT_PORT_REQUEST "PRTREQ" // Inviato al server per richiedere la porta di ascolto di un altro client
#define GROUP_CHAT_INVITE "GRPINVITE" // Invito per l'aggiunta alla chat di gruppo, spedito all'utente che si vuole aggiungere
#define YES "Y" // Utilizzato (dall'utente invitato) per rispondere positivamente all'invito nella chat di gruppo
#define NO "N" // Utilizzato (dall'utente invitato) per rispondere negativamente all'invito nella chat di gruppo
#define END_MEMBERS "ENDUSR" // Inviato al nuovo partecipante della chat per indicare la fine dell'invio dei membri della chat
#define MEMBER_PORT_REQUEST "GRPPRTREQ" // Inviato dal nuovo partecipante al server per ricevere le porte di ascolto dei membri della chat
#define NEW_MEMBER "NEWMBR" // Inviato dal nuovo partecipante a tutti i membri della chat di gruppo

/*
 * 1) Si invia il comando di hanging al server
 * 2) Il server invia una serie di risposte, ognuna contenente le informazioni sui messaggi pendenti raggruppate per mittente
 * 3) Il server invia il segnale che indica che i messaggi pendenti sono finiti
 */
#define HANGING_COMMAND "HNG" // Inviato al server quando l'utente vuole avere le informazioni sui messaggi pendenti
#define DONE_HANGING "ENDHNG" // Inviato dal server quando i messaggi pendenti sono finiti

/*
 * 1) Si invia il comando di show al server
 * 2) Si invia al server l'username del mittente di cui si vogliono leggere i messaggi
 * 3) Il server invia i messaggi pendenti
 * 4) Il server invia il segnale che sono finiti i messaggi pendenti
 * 5) Il server notifica al mittente dei messaggi, se online, che i suoi messaggi pendenti
 *    sono stati inviati al destinatario (inviando successivamente l'username del destinatario)
 */
#define SHOW_COMMAND "SHW" // Inviato al server quando l'utente esegue il comando 'show'
#define DONE_SHOW "ENDSHW" // Inviato dal server quando sono terminati i messaggi pendenti
#define MESSAGES_SENT "SENT" // Inviato dal server quando i messaggi pendenti sono stati recapitati al destinatario

/*
 * Segnala al server che si sta per inviare un nuovo messaggio di una chat che non può essere
 * recapitato al interlocutore poiché offline.
 * Dopo questo comando si invia il messaggio vero e proprio. Non è necessario inviare anche il
 * mittente poiché lo troverà dal registro (contenente tutti gli utenti che si sono collegati).
 */
#define OFFLINE_MESSAGE "NEWMSG"
#define LOGGED_MSG "OKMSG" // Inviato per segnalare il completamento della registrazione del messaggio sul file di log