/**********************************************
 *                                            *
 *              Codice del server             *
 *                                            *
 **********************************************/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <time.h>
#include "struct/registro.h"
#include "costanti.h"
#include "util/messaggi.h"
#include "util/string.h"
#include "util/time.h"
#include "util/file.h"

int server_socket, new_sd, len;
struct sockaddr_in server_addr, client_addr;
fd_set master;
struct record_registro* registro; // Registro del login/logout degli utenti

/*
 * Verifica se l'utente specificato è nel registro. Se è presente viene restituito
 * il record corrispondente nel registro, altrimenti 0.
 */
struct record_registro* find_user_in_register(char* username) {
    struct record_registro* record;

    // Controllo se ci sono utenti registrati
    if (!registro)
        return 0;

    // Scorro il registro del server
    for (record = registro; record != NULL; record = record->next)
        if (strcmp(record->username, username) == 0)
            return record;

    return 0;
}

/*
 * Controlla se l'utente specificato è online. Se lo è restituisce 1, altrimenti 0.
 */
int is_user_online(char* user) {

    // Cerco l'utente nel registro
    struct record_registro* record = find_user_in_register(user);
    if (record == 0)
        return 0;

    // L'utente è online se il timestamp di logout è 0
    return record->logout_timestamp == 0 ? 1 : 0;
}

/*
 * Registra sul file di log il login/logout ('operazione') dell'utente
 */
void log_user_activity(char* username, char* operazione) {
    char timestamp[TIMESTAMP_LEN];
    FILE* log;

    // Apro il file in append
    log = open_or_create(ACTIVITY_LOG_FILE, "a");
    if (log == NULL)
        return; // Impossibile accedere al file

    // Registro l'attività dell'utente al timestamp corrente
    format_timestamp(time(NULL), timestamp, sizeof(timestamp));
    fprintf(log, "[%s] %s di %s\n", timestamp, operazione, username);
    if (fclose(log) != 0)
        fprintf(stderr, "Errore durante la chiusura del file di log '%s' : %s\n", ACTIVITY_LOG_FILE, strerror(errno));

    #ifdef DEBUG
    printf("[%s] Login di '%s' registrato sul file '%s'.\n", timestamp, username, ACTIVITY_LOG_FILE);
    #endif
}

/*
 * Cerca l'username associato al socket specificato e lo pone in 'username'.
 * Se non lo trova si pone solo il terminatore di stringa (\0).
 */
void find_username_from_socket(int socket, char* username) {
    struct record_registro* appoggio;

    // Controllo se ci sono utenti registrati
    if (!registro) {
        username[0] = '\0';
        return;
    }

    // Scorro il registro del server
    for (appoggio = registro; appoggio != NULL; appoggio = appoggio->next) {
        if (appoggio->socket == socket) {
            strcpy(username, appoggio->username);
            return;
        }
    }
    username[0] = '\0';
}

/*
 * Cerca il socket a cui è connesso l'utente con l'username specificato.
 * Se lo trova restituisce il socket, altrimenti -1.
 */
int find_socket_from_username(char* username) {
    struct record_registro* record = find_user_in_register(username); // Cerco l'username nel registro
    return record == 0 ? -1 : record->socket;
}

/*
 * Stampa il registro del server
 */
void print_register(void) {
    char timestamp[TIMESTAMP_LEN];
    struct record_registro* appoggio;

    // Controllo se ci sono utenti registrati
    if (!registro) {
        printf("Nessun utente si è ancora collegato al server :(\n");
        return;
    }

    printf("**********************************\n");
    printf("Registro:\n");
    for (appoggio = registro; appoggio != NULL; appoggio = appoggio->next) { // Scorro il registro del server
        printf("-- Username: %s\n", appoggio->username);
        printf("Socket: %d\n", appoggio->socket);

        format_timestamp(appoggio->login_timestamp, timestamp, sizeof(timestamp));
        printf("Login: %s\n", timestamp);

        // Se logout == 0, l'utente è online
        format_timestamp(appoggio->logout_timestamp, timestamp, sizeof(timestamp));
        printf("Logout: %s\n", appoggio->logout_timestamp != 0 ? timestamp : "NULL");
    }
    printf("**********************************\n");
}

/*
 * Aggiorna il timestamp di logout dell'utente al timestamp corrente
 */
void update_logout_timestamp(char* username) {

    // Cerco l'username nel registro
    struct record_registro* record = find_user_in_register(username);
    if (record != 0) {
        record->logout_timestamp = time(NULL); // Timestamp corrente
        record->socket = INVALID_SOCKET; // Socket inesistente
    }

    #ifdef DEBUG
    print_register(); // Stampa il registro del server
    #endif
}

/*
 * Chiude il socket di un client che si è disconnesso (recv ha restituito 0)
 * e imposta il timestamp di logout (pari al timestamp corrente)
 */
void client_disconnection(int socket) {
    char username[USERNAME_LEN];

    close(socket);
    FD_CLR(socket, &master);

    // Aggiorno il timestamp di logout
    find_username_from_socket(socket, username);
    if (username[0] != '\0') {
        update_logout_timestamp(username);
        log_user_activity(username, "LOGOUT");

        #ifdef DEBUG
        printf("Utente '%s' disconnesso dal server.\n", username);
        #endif
    } else {
        #ifdef DEBUG
        printf("Disconnessione di un client: l'username associato al socket non è stato trovato nel registro. Se il socket è stato chiuso prima che l'utente abbia eseguito il login, è tutto ok.\n");
        #endif
    }
}

/*
 * Stampa la lista dei comandi
 */
void print_auth_commands(void) {
    printf("--------- COMANDI DISPONIBILI ---------\n");
    printf("1) help -> mostra i dettagli dei comandi\n");
    printf("2) list -> mostra un elenco degli utenti connessi\n");
    printf("3) esc -> chiude il server\n");
}

/*
 * Comando 'help': dettaglia i comandi disponibili
 */
void help(void) {
    printf("**********************************\n");
    printf("GUIDA SUI COMANDI:\n");
    printf("1) help -> Mostra questo menù\n");
    printf("2) list -> Mostra l’elenco degli utenti connessi, indicando username, timestamp di connessione e numero di porta nel formato \"username*timestamp*porta\"\n");
    printf("3) esc -> Termina il server. La terminazione del server non impedisce alle chat in corso di proseguire. Se il server è disconnesso, nessun utente può più fare login. Gli utenti che si disconnettono in seguito a ciò salvano l'istante di disconnessione, per poi mandarlo al server quando entrambe le parti tornano online\n");
    printf("**********************************\n");
}

/*
 * Comando 'list': mostra gli utenti connessi
 */
void list(void) {
    char timestamp[TIMESTAMP_LEN]; // Contiene il timestamp formattato
    struct record_registro* record;

    // Controllo se ci sono utenti registrati
    if (!registro) {
        printf("Nessun utente si è ancora collegato al server :(\n");
        return;
    }

    printf("**********************************\n");
    printf("Elenco di utenti online (username*timestamp di login*porta):\n");

    // Scorro il registro del server
    for (record = registro; record != NULL; record = record->next) {
        if (record->logout_timestamp != 0) // L'utente è online se logout == 0
            continue;

        /*
         * Converte il timestamp nel formato "giorno-mese-anno"
         * Fonte: https://stackoverflow.com/a/3673291
         */
        strftime(timestamp, sizeof(timestamp), "%d-%B-%Y", localtime(&record->login_timestamp));

        printf("%s*%s*%d\n", record->username, timestamp, record->port);
    }
    printf("**********************************\n");
}

/*
 * Comando 'esc': termina il server
 */
void esc(void) {
    printf("Chiusura del server in corso...\n");
    sleep(3);
    close(server_socket);
    exit(0);
}

/*
 * Aspetto le credenziali (username e password) e la porta dal client (sul socket specificato).
 * Una volta ricevute vengono messe in 'username', 'password' e 'porta'.
 */
void credential_reception(int socket, char* username, char* password, int* porta) {
    int ret;

    // Ricevo l'username
    ret = receive_string(socket, username);
    if (ret <= 0) { // Errore o disconnessione del client
        if (ret == 0)
            client_disconnection(socket);
        return;
    }

    // Ricevo la password
    ret = receive_string(socket, password);
    if (ret <= 0) { // Errore o disconnessione del client
        if (ret == 0)
            client_disconnection(socket);
        return;
    }

    #ifdef DEBUG
    printf("Username: '%s', password: '%s'.\n", username, password);
    #endif

    // Ricevo la porta di ascolto del client
    ret = receive_integer(socket, porta);
    if (ret <= 0) { // Errore o disconnessione del client
        if (ret == 0)
            client_disconnection(socket);
        return;
    }
}

/*
 * Stampa il file contenente i messaggi pendenti
 */
void print_hanging_list(void) {
    FILE* file; // File contenente tutti i messaggi pendenti ancora da recapitare
    char line[MAX_LINE_LEN]; // Riga letta dal file

    file = open_file(OFFLINE_MSG_FILE, "r");
    if (file == NULL)
        return; // File inesistente/inaccessibile

    printf("Contenuto del file '%s':\n", OFFLINE_MSG_FILE);

    // Stampo riga per riga tutto il file
    for (;;) {
        if (fgets(line, MAX_LINE_LEN, file) == NULL)
            break; // Fine file

        printf("'%s'\n", line);
    }

    if (fclose(file) != 0)
        fprintf(stderr, "Errore durante la chiusura del file '%s' : %s\n", OFFLINE_MSG_FILE, strerror(errno));
}

/*
 * Creo una riga per l'utente specificato nel file dei messaggi pendenti.
 * In questa riga saranno memorizzati i messaggi a lui inviati mentre è offline.
 */
void insert_into_hanging_list(char* username) {
    FILE* file; // File contenente i messaggi pendenti (inviati mentre un giocatore è offline)

    // Apro il file in append
    file = open_or_create(OFFLINE_MSG_FILE, "a");
    if (file == NULL)
        return; // Impossibile accedere al file

    fprintf(file, "%s\nlist:\n", username);

    #ifdef DEBUG
    printf("Username '%s' scritto sul file dei messaggi pendenti.\n", username);
    #endif

    if (fclose(file) != 0)
        fprintf(stderr, "Errore durante la chiusura del file '%s' : %s\n", OFFLINE_MSG_FILE, strerror(errno));

    #ifdef DEBUG
    print_hanging_list();
    #endif
}

/*
 * Comando 'signup' lato client: registra l'utente
 */
void signup(int socket) {
    int ret;
    char username[USERNAME_LEN]; // Username ricevuto dal client
    char password[PASSWORD_LEN]; // Password ricevuta dal client
    int client_port; // Porta di ascolto del client
    FILE* file; // File contenente tutti gli utenti registrati (con le relative password)
    char line[MAX_LINE_LEN]; // Riga letta dal file

    // Recupero username, password e porta di ascolto inviate dal client
    credential_reception(socket, username, password, &client_port);

    file = open_or_create(USERS_FILE, "r");
    if (file == NULL)
        return; // Impossibile accedere al file

    /*
     * Verifico se l'username è già stato preso da un altro utente: leggo tutto
     * il file (riga per riga) finché non trovo 'username' (se presente)
     */
    for (;;) {
        if (fgets(line, MAX_LINE_LEN, file) == NULL)
            break; // Fine file
        else {
            // Recupero l'username dalla riga
            strcpy(line, strtok(line, " "));
            if (strcmp(line, username) == 0) { // Se ho già un utente con quel nome
                #ifdef DEBUG
                printf("Esiste già un utente con l'username '%s'!\n", username);
                #endif

                // Segnala al client che esiste già un utente con quell'username
                ret = send_string(socket, ALREADY_EXISTING_USERNAME);
                if (ret < 0) { // Errore
                    if (fclose(file) != 0)
                        fprintf(stderr, "Errore durante la chiusura del file '%s' : %s\n", USERS_FILE, strerror(errno));
                    return;
                }

                if (fclose(file) != 0)
                    fprintf(stderr, "Errore durante la chiusura del file '%s' : %s\n", USERS_FILE, strerror(errno));
                return;
            }
        }
    }
    if (fclose(file) != 0)
        fprintf(stderr, "Errore durante la chiusura del file '%s' : %s\n", USERS_FILE, strerror(errno));

    // Apro il file in append
    file = open_file(USERS_FILE, "a");
    if (file == NULL)
        return; // File inaccessibile

    // Scrivo l'utente sul file
    fprintf(file, "%s %s\n", username, password);

    #ifdef DEBUG
    printf("L'utente '%s' si è registrato.\n", username);
    #endif

    // Segnalo al client che l'utente è stato registrato con successo
    ret = send_string(socket, SIGNED_UP);
    if (ret < 0) { // Errore
        if (fclose(file) != 0)
            fprintf(stderr, "Errore durante la chiusura del file '%s' : %s\n", USERS_FILE, strerror(errno));
        return;
    }

    if (fclose(file) != 0)
        fprintf(stderr, "Errore durante la chiusura del file '%s' : %s\n", USERS_FILE, strerror(errno));

    // Creo una riga per l'utente nel file che contiene i messaggi pendenti
    insert_into_hanging_list(username);
}

/*
 * Verifica che il comando (lato server) esista e lo esegue
 */
void run_server_command(char* buffer) {
    remove_new_line(buffer); // Sostituisco il carattere new-line (\n) con il terminatore di stringa (\0)

    #ifdef DEBUG
    // Comando 'registro': stampa la lista degli utenti registrati
    if (strcmp(buffer, "registro") == 0) {
        print_register();
    } else
    #endif

    if (strcmp("help", buffer) == 0)
        help();
    else if (strcmp("list", buffer) == 0)
        list();
    else if (strcmp("esc", buffer) == 0)
        esc();
    else {
        printf("Comando non valido!\n");
        print_auth_commands();
    }
}

/*
 * Riceve la stringa 'list' derivante dal file con i messaggi pendenti e la elabora per mandare
 * a chi ha eseguito il comando 'hanging' le informazioni richieste
 */
void send_pending_messages(int socket, char* list) {
    int ret, i, j = 0, user_found = 0, number_found = 0;
    char user[USERNAME_LEN];
    char numero[MAX_MSG_LEN]; // Numero messaggi pendenti
    char time[MAX_MSG_LEN]; // Timestamp ultimo messaggio

    for (i = 5; i < MAX_LINE_LEN; i++) { // i=5 perché ho 'list:' in testa
        if (list[i] == ':') { // Ho terminato di leggere un campo
            if (user_found == 0) { // Ho finito di leggere l'utente
                user[j] = '\0';
                user_found = 1;
                j = 0;

                // Invio l'username
                ret = send_string(socket, user);
                if (ret < 0) // Errore
                    return;

                continue;
            } else if (number_found == 0) { // Ho finito di leggere il numero di messaggi pendenti
                number_found = 1;
                numero[j] = '\0';
                j = 0;

                // Invio il numero di messaggi pendenti
                ret = send_string(socket, numero);
                if (ret < 0) // Errore
                    return;

                continue;
            } else { // Ho finito di leggere il timestamp
                time[j] = '\0';
                user_found = 0;
                number_found = 0;
                j = 0;

                // Invio il timestamp
                ret = send_string(socket, time);
                if (ret < 0) // Errore
                    return;

                continue;
            }
        }

        if (user_found == 0) // Sto leggendo il mittente
            user[j++] = list[i];
        else if (number_found == 0) // Sto leggendo il numero di messaggi pendenti
            numero[j++] = list[i];
        else // Sto leggendo il timestamp del messaggio più recente
            time[j++] = list[i];

        // Linea (= messaggi pendenti) finita
        if (list[i] == '\0') {
            ret = send_string(socket, DONE_HANGING);
            if (ret < 0) // Errore
                return;

            break;
        }
    }
}

/*
 * Implementa la funzionalità di hanging: cerca tra i messaggi pendenti quelli indirizzati a 'destinatario'
 * e li invia sul socket specificato, raggruppati per mittente.
 */
void hanging(int socket, char* destinatario) {
    int i = 0, j = 0;
    FILE* file; // File contenente i messaggi pendenti
    FILE* tmp_file; // File temporaneo di appoggio
    char tmp_file_path[PATH_MAX]; // Path del file temporaneo
    char line[MAX_LINE_LEN]; // Riga letta dal file

    // Se il file non esiste, allora non ci sono messaggi pendenti
    file = open_file(OFFLINE_MSG_FILE, "r");
    if (file == NULL) {
        send_string(socket, DONE_HANGING);
        return;
    }

    // Apro il scrittura il file di appoggio
    strcpy(tmp_file_path, OFFLINE_MSG_FILE);
    strcat(tmp_file_path, "_bk.txt");
    tmp_file = open_or_create(tmp_file_path, "w");
    if (tmp_file == NULL)
        return; // File inaccessibile

    // Scorro il file riga per riga alla ricerca del destinatario dei messaggi
    for (;;) {
        if (fgets(line, MAX_LINE_LEN, file) == NULL)
            break; // Fine file

        // Sostituisco il carattere new-line (\n) nella riga letta con il terminatore di stringa (\0)
        remove_new_line(line);

        // Controllo se sono alla riga di 'destinatario'
        if (strcmp(line, destinatario) == 0 && i % 2 == 0) {
            /* La prossima riga contiene i campi da inviare */

            j = 1;
            fprintf(tmp_file, "%s\n", line);
        } else if (j == 1) {
            // Invio le informazioni sui messaggi pendenti
            send_pending_messages(socket, line);
            fprintf(tmp_file, "%s\n", "list:");
            j = 0;
        } else
            fprintf(tmp_file, "%s\n", line); // Mantengo la riga originale

        i++;
    }

    if (fclose(file) != 0)
        fprintf(stderr, "Errore durante la chiusura del file dei messaggi pendenti '%s' : %s\n", OFFLINE_MSG_FILE,
                strerror(errno));
    if (fclose(tmp_file) != 0)
        fprintf(stderr, "Errore durante la chiusura del file temporaneo '%s' : %s\n", tmp_file_path, strerror(errno));

    // Elimino il vecchio file di log e rinomino quello temporaneo (che prenderà il posto del vecchio)
    if (remove(OFFLINE_MSG_FILE) == -1)
        perror("Errore in hanging() durante la cancellazione del vecchio file dei messaggi pendenti");
    if (rename(tmp_file_path, OFFLINE_MSG_FILE) == -1)
        perror("Errore mentre si tentava di rinominare il file di log temporaneo");
}

/*
 * Inserisce l'utente, il socket e la porta specificati nel registro del server
 */
void add_to_register(char* username, int socket, int client_port) {
    struct record_registro* appoggio = registro;
    struct record_registro* new_user;

    // Se la lista è vuota
    if (registro == NULL) {
        // Inizializzo la lista e inserisco il nuovo elemento
        registro = (void*) malloc(sizeof(struct record_registro));
        strcpy(registro->username, username);
        registro->socket = socket;
        registro->login_timestamp = time(NULL); // Timestamp corrente
        registro->logout_timestamp = 0; // Utente online
        registro->port = client_port;
        registro->next = NULL;
    } else {
        // Trovo la coda della lista (posizione vuota)
        while (appoggio->next != NULL)
            appoggio = appoggio->next;

        // Alloco e inserisco il nuovo elemento
        new_user = (void*) malloc(sizeof(struct record_registro));
        strcpy(new_user->username, username);
        new_user->socket = socket;
        new_user->login_timestamp = time(NULL); // Timestamp corrente
        new_user->logout_timestamp = 0; // Utente online
        new_user->port = client_port;
        new_user->next = NULL;
        appoggio->next = new_user;
    }

    #ifdef DEBUG
    print_register(); // Stampa il registro del server
    #endif
}

/*
 * Aggiorna il record di 'user' nel registro del server con il socket e la porta specificati.
 * Inoltre imposta il timestamp di login al timestamp corrente e il timestamp di logout a 0 (= utente online).
 */
void update_register_entry(int socket, char* user, int porta_client) {
    struct record_registro* appoggio;

    for (appoggio = registro; appoggio != NULL; appoggio = appoggio->next) {
        if (strcmp(appoggio->username, user) == 0) {
            appoggio->login_timestamp = time(NULL); // Timestamp corrente
            appoggio->logout_timestamp = 0; // Utente online
            appoggio->socket = socket;
            appoggio->port = porta_client;
            break;
        }
    }

    #ifdef DEBUG
    print_register(); // Stampa il registro del server
    #endif
}

/*
 * Implementa la funzionalità di login: controlla che l'username esista e che la password sia corretta.
 * Notifica poi a tutti i client che un nuovo utente è online.
 */
void in(int socket) {
    int ret;
    char username[USERNAME_LEN];
    char password[PASSWORD_LEN];
    int client_port; // Porta di ascolto del client
    FILE* users; // File contenente tutti gli utenti registrati (e le relative password)
    char line[MAX_LINE_LEN]; // Riga letta dal file
    char tmp_user[USERNAME_LEN]; // Username nella riga letta
    char tmp_pass[PASSWORD_LEN]; // Password nella riga letta
    char appoggio[MAX_MSG_LEN];
    int found = 0; // Indica se esiste un utente con l'username specificato
    struct record_registro* record;

    // Recupero l'username, la password e la porta di ascolto dal client
    credential_reception(socket, username, password, &client_port);

    // Se il file non esiste, sicuramente non c'è nessun utente registrato
    if (is_file_existing(USERS_FILE) == 0) {
        ret = send_string(socket, UNKNOWN_USER); // Invio risposta: username non trovato (non esiste)
        if (ret < 0) // Errore
            return;
        return;
    }

    // Apro il file in lettura
    users = open_file(USERS_FILE, "r");
    if (users == NULL)
        return; // File inaccessibile

    // Leggo il file riga per riga alla ricerca dell'username specificato
    for (;;) {
        if (fgets(line, MAX_LINE_LEN, users) == NULL)
            break; // Fine file

        // Ricavo l'username e la password dalla riga
        strcpy(tmp_user, strtok(line, " "));
        strcpy(tmp_pass, strtok(NULL, "\n")); // Arriva fino a fine stringa così da consentire passphrase
        remove_new_line(tmp_pass); // Sostituisco il carattere new-line (\n) con il terminatore di stringa (\0)

        #ifdef DEBUG
        printf("Username: '%s', password: '%s'.\n", tmp_user, tmp_pass);
        #endif

        // Controllo se l'username nella riga corrisponde a quello inviato dal client
        if (strcmp(tmp_user, username) == 0) {
            found = 1;
            break;
        }
    }

    if (found == 0) { // Username non trovato (non esiste)
        ret = send_string(socket, UNKNOWN_USER);
        if (ret < 0) // Errore
            return;
        return;
    }

    if (strcmp(password, tmp_pass) != 0) { // Password errata
        ret = send_string(socket, WRONG_PASSWORD);
        if (ret < 0) // Errore
            return;
        return;
    }

    /* Password corretta */

    // Inserisco l'utente nel registro o aggiorno il suo record (se c'è già)
    if (find_user_in_register(username) == 0)
        add_to_register(username, socket, client_port);
    else
        update_register_entry(socket, username, client_port);

    // Invio risposta: credenziali corrette, login avvenuto con successo
    ret = send_string(socket, AUTHENTICATED);
    if (ret < 0) // Errore
        return;

    // Registro il login dell'utente nel file di log
    log_user_activity(username, "LOGIN");

    // Controllo se il registro è vuoto
    if (!registro)
        return;

    // Comunico a tutti i client online il login del nuovo client
    for (record = registro; record != NULL; record = record->next) {

        // Se il client è online e non è il client che si è appena connesso
        if (record->logout_timestamp == 0 && strcmp(record->username, username) != 0) {

            // Invio il segnale che notifica che un nuovo utente è ora online
            strcpy(appoggio, NOW_ONLINE);
            ret = send_string(record->socket, appoggio);
            if (ret < 0) // Errore
                continue;

            // Invio l'username dell'utente che si è appena collegato
            ret = send_string(record->socket, username);
            if (ret < 0) // Errore
                continue;

            // Invio la porta dell'utente che si è appena collegato
            ret = send_integer(record->socket, client_port);
            if (ret < 0) // Errore
                continue;
        }
    }

    #ifdef DEBUG
    printf("'%s' ha eseguito il login.\n", username);
    #endif
}

/*
 * Funzione invocata quando un client vuole creare una chat di gruppo.
 * Si occupa di ricevere una serie di username dal client e rispondere
 * dicendo se sono online o meno.
 */
void group_chat(int socket) {
    int ret;
    char buffer[MAX_MSG_LEN];
    struct record_registro* utente;

    for (;;) {
        ret = receive_string(socket, buffer);
        if (ret <= 0) { // Errore o disconnessione del client
            if (ret == 0)
                client_disconnection(socket);
            return;
        }

        if (strcmp(buffer, GROUP_CHAT_DONE) == 0)
            break;

        // Si informa il client se l'utente è online o offline
        utente = find_user_in_register(buffer);
        ret = send_string(socket, utente == 0 || utente->logout_timestamp != 0 ? USER_OFFLINE : USER_ONLINE);
        if (ret < 0) // Errore
            return;
    }
}

/*
 * Funzione invocata quando un client vuole inserire un utente in una chat di gruppo.
 * Si occupa di ricevere l'username di cui il client vuole conoscere la porta e risponde
 * dicendo innanzitutto se l'utente è ancora online e, se lo è, qual è la sua porta di ascolto.
 */
void insert_into_group_chat(int socket) {
    int ret;
    char buffer[MAX_MSG_LEN];
    struct record_registro* utente;

    // Si ricevere l'username di cui si vuole conoscere la porta
    ret = receive_string(socket, buffer);
    if (ret <= 0) { // Errore o disconnessione del client
        if (ret == 0)
            client_disconnection(socket);
        return;
    }

    /*
     * Se l'utente è online, si notifica e si invia la porta.
     * Se l'utente è offline, si notifica ciò e basta.
     */
    utente = find_user_in_register(buffer);
    if (utente == 0 || utente->logout_timestamp != 0) {
        ret = send_string(socket, USER_OFFLINE);
        if (ret < 0) // Errore
            return;
    } else {
        ret = send_string(socket, USER_ONLINE);
        if (ret < 0) // Errore
            return;

        ret = send_integer(socket, utente->port);
        if (ret < 0) // Errore
            return;
    }
}

/*
 * Aggiunge al file di log delle show una notifica di avvenuta consegna di messaggi pendenti
 * che non è stata consegnata poiché il mittente dei messaggi recapitati è offline
 */
void add_show_notify(char* mittente, char* destinatario) {
    FILE* file;
    char appoggio[MAX_LINE_LEN];

    // Apro il file in append
    file = open_or_create(SHOW_LOG_FILE, "a");
    if (file == NULL)
        return; // File inaccessibile

    // Registro la notifica pendente
    strcpy(appoggio, mittente); // Mittente dei messaggi: dovrà ricevere la notifica
    strcat(appoggio, ":");
    strcat(appoggio, destinatario); // Ricevente dei messaggi: ha ricevuto i messaggi pendenti
    fprintf(file, "%s\n", appoggio);

    if (fclose(file) != 0)
        fprintf(stderr, "Errore durante la chiusura del file '%s' : %s\n", SHOW_LOG_FILE, strerror(errno));
}

/*
 * Notifica a 'mittente' l'invio di un messaggio che aveva mandato quando 'destinatario' era offline
 */
void notify_reception(char* mittente, char* destinatario) {
    int ret;
    struct record_registro* record = find_user_in_register(mittente);

    // Se il mittente dei messaggi recapitati è online invio la notifica di invio
    if (record != 0 && record->logout_timestamp == 0) {

        // Invio la notifica di invio dei messaggi pendenti
        ret = send_string(record->socket, MESSAGES_SENT);
        if (ret < 0) //Errore
            return;

        // Invio il destinatario che ha ricevuto i messaggi pendenti
        ret = send_string(record->socket, destinatario);
        if (ret < 0) //Errore
            return;
    } else // Il mittente dei messaggi è offline: devo salvare la notifica da inviargli
        add_show_notify(mittente, destinatario);
}

/*
 * Invia a 'mittente' la notifica di ricezione dei messaggi pendenti che aveva inviato
 * a 'destinatario' mentre questo era offline
 */
void send_past_show(char* mittente, char* destinatario) {
    FILE* show_log;
    FILE* tmp_file; // File temporaneo di appoggio
    char tmp_file_path[PATH_MAX]; // Path del file temporaneo
    char line[MAX_LINE_LEN]; // Riga letta dal file
    char appoggio[MAX_LINE_LEN];

    // Se il file non esiste sicuramente non ci sono notifiche da inviare
    if (is_file_existing(SHOW_LOG_FILE) == 0)
        return;

    // Apro il file in lettura
    show_log = open_file(SHOW_LOG_FILE, "r");
    if (show_log == NULL)
        return; // File inaccessibile

    // Apro il scrittura il file di appoggio
    strcpy(tmp_file_path, SHOW_LOG_FILE);
    strcat(tmp_file_path, "_bk.txt");
    tmp_file = open_or_create(tmp_file_path, "w");
    if (tmp_file == NULL)
        return; // File inaccessibile

    strcpy(appoggio, mittente);
    strcat(appoggio, ":");
    strcat(appoggio, destinatario);

    // Scorro il file riga per riga alla ricerca di 'mittente' e 'destinatario'
    for (;;) {
        if (fgets(line, MAX_LINE_LEN, show_log) == NULL)
            break; // Fine file

        // Sostituisco il carattere new-line (\n) con il terminatore di stringa (\0)
        remove_new_line(line);

        // Verifico se sono alla linea corretta ('mittente:destinatario')
        if (strncmp(line, appoggio, strlen(appoggio)) == 0)
            notify_reception(mittente, destinatario); // Invio la notifica
        else
            fprintf(tmp_file, "%s\n", line); // Lascio la riga inalterata
    }

    if (fclose(show_log) != 0)
        fprintf(stderr, "Errore durante la chiusura del file '%s' : %s\n", SHOW_LOG_FILE, strerror(errno));
    if (fclose(tmp_file) != 0)
        fprintf(stderr, "Errore durante la chiusura del file temporaneo '%s' : %s\n", tmp_file_path, strerror(errno));

    // Elimino il vecchio file e rinomino quello temporaneo (che prenderà il posto del vecchio)
    if (remove(SHOW_LOG_FILE) == -1)
        perror("Errore durante la cancellazione del vecchio file contenente le notifiche di show pendenti");
    if (rename(tmp_file_path, SHOW_LOG_FILE) == -1)
        perror("Errore mentre si tentava di rinominare il file di log temporaneo");
}

/*
 * Invocata quando un client vuole avviare una nuova chat 1-to-1.
 * Si occupa di ricevere l'username con cui si vuole avviare la chat e di controllare
 * se è online o meno (comunicandolo all'utente che vuole avviare la chat).
 */
void chat(int socket) {
    int ret, i;
    char username[USERNAME_LEN]; // Utente che vuole avviare la chat
    char destinatario[USERNAME_LEN]; // Utente con cui si vuole avviare la chat
    struct record_registro* record; // Record nel registro dell'utente con cui si vuole conversare

    // Ricevo l'username dell'utente con cui si vuole avviare una chat
    ret = receive_string(socket, destinatario);
    if (ret <= 0) { // Errore o disconnessione del client
        if (ret == 0)
            client_disconnection(socket);
        return;
    }
    record = find_user_in_register(destinatario);

    // Trovo l'username del mittente (che vuole avviare una chat)
    find_username_from_socket(socket, username);
    if (username[0] == '\0') {
        printf("Errore in chat(): nessun utente associato al socket.\n");
        return;
    }

    /*
     * Se il destinatario è offline, lo comunico al mittente.
     * Se è online, lo comunico e invio la porta di ascolto del device.
     * Se l'invio della comunicazione fallisce, si prova ad inviarla per 3
     * volte (come da specifiche).
     */
    if (record == 0 || record->logout_timestamp != 0) {
        for (i = 0; i < 3; i++) {
            ret = send_string(socket, USER_OFFLINE);
            if (ret >= 0) // Send andata a buon fine
                break;
        }

        if (ret < 0) // Errore in tutti i tentativi
            return;
    } else {
        for (i = 0; i < 3; i++) {
            ret = send_string(socket, USER_ONLINE);
            if (ret >= 0) // Send andata a buon fine
                break;
        }
        if (ret < 0) // Errore in tutti i tentativi
            return;

        ret = send_integer(socket, record->port);
        if (ret < 0) // Errore
            return;
    }

    // Se ce ne sono, notifico che i messaggi pendenti inviati dal mittente sono stati consegnati al destinatario
    send_past_show(username, destinatario);
}

/*
 * Implementa la funzionalità di show: invia i messaggi pendenti e aggiorna il log della chat
 * inserendo il segno che indica che il messaggio è stato letto (quest'ultima parte è una
 * semplificazione ottenuta dalla condivisione dei log delle chat tra client e server).
 */
void show(int socket) {
    int ret;
    char esecutore[USERNAME_LEN]; // Utente che ha eseguito la show()
    char mittente[USERNAME_LEN]; // Utente che ha inviato i messaggi pendenti che si vogliono leggere
    char appoggio[USERNAME_LEN];
    char path[PATH_MAX]; // File contenente i log della chat
    char tmp_file_path[PATH_MAX]; // File temporaneo
    FILE* log; // File contenente i log della chat
    FILE* log_tmp; // File temporaneo
    char linea[MAX_LINE_LEN]; // Riga letta da file
    char out[MAX_LINE_LEN]; // Riga da scrivere sul file
    int none_sent = 1; // Indica se sono stati trovati o meno messaggi pendenti

    // Trovo l'utente che ha inviato la show grazie al socket che è stato utilizzato per inviare il comando di show
    find_username_from_socket(socket, esecutore);
    if (esecutore[0] == '\0') {
        printf("Errore nella show(): nessun utente associato al socket.\n");
        return;
    }

    // Ricevo il mittente dei messaggi pendenti che si vogliono leggere
    ret = receive_string(socket, mittente);
    if (ret <= 0) { // Errore o disconnessione del client
        if (ret == 0)
            client_disconnection(socket);
        return;
    }

    /*
     * Se il file non viene trovato, provo a scambiare l'ordine degli username nel nome del file.
     * Ad esempio, il file di log può essere pippo-pluto.txt ma anche pluto-pippo.txt.
     */
    get_chat_log_path(mittente, esecutore, path);
    if (is_file_existing(path) == 0) // Il file di log non esiste
        get_chat_log_path(esecutore, mittente, path);

    // Se non c'è uno storico della chat, sicuramente non ci sono messaggi pendenti
    log = open_file(path, "r");
    if (log == NULL) {
        send_string(socket, DONE_SHOW);
        return;
    }

    // File di appoggio temporaneo
    strcpy(tmp_file_path, path);
    strcat(tmp_file_path, "_tmp.txt");

    // Apro il file temporaneo in modalità append
    log_tmp = open_or_create(tmp_file_path, "a");
    if (log_tmp == NULL) { // Impossibile accedere al file
        send_string(socket, DONE_SHOW);

        if (fclose(log) != 0)
            fprintf(stderr, "Errore durante la chiusura del file di log della chat '%s' : %s\n", path, strerror(errno));

        return;
    }

    // Appoggio contiene 'mittente:'
    strcpy(appoggio, mittente);
    strcat(appoggio, ":");

    for (;;) { // Scorro il file di log contenente lo storico della chat
        if (fgets(linea, MAX_LINE_LEN, log) == NULL)
            break; // File terminato

        // Se il messaggio è stato letto
        if (strstr(linea, READ_MARK) != NULL) {
            fprintf(log_tmp, "%s", linea); // Lascio la riga inalterata
            continue;
        }

        /* Messaggio non letto */

        // Aggiorno solo le line che sono da parte del mittente
        if (strncmp(linea, appoggio, strlen(appoggio)) == 0) {
            memset(out, 0, sizeof(out)); // Ripulisco ogni volta il buffer

            // Inserisco il segno per segnalare che adesso il messaggio è letto
            strncpy(out, linea, strstr(linea, UNREAD_MARK) - linea);
            strcat(out, READ_MARK);
            fprintf(log_tmp, "%s\n", out); // Scrivo la nuova riga sul file temporaneo

            #ifdef DEBUG
            printf("Segnato il messaggio '%s' come letto nel file '%s'.", out, path);
            #endif

            // Mando al client i messaggi pendenti che aveva
            ret = send_string(socket, out);
            if (ret < 0) // Errore
                continue;

            none_sent = 0;
        } else
            fprintf(log_tmp, "%s", linea); // Lascio inalterata la riga
    }

    if (fclose(log) != 0)
        fprintf(stderr, "Errore durante la chiusura del file di log della chat '%s' : %s\n", path, strerror(errno));
    if (fclose(log_tmp) != 0)
        fprintf(stderr, "Errore durante la chiusura del file temporaneo di log '%s' : %s\n", tmp_file_path,
                strerror(errno));

    // Elimino il vecchio file di log e rinomino quello temporaneo (che prenderà il posto del vecchio)
    if (remove(path) == -1)
        perror("Errore durante la cancellazione del vecchio file di log della chat");
    if (rename(tmp_file_path, path) == -1)
        perror("Errore mentre si tentava di rinominare il file di log temporaneo");

    // Comunico al client che sono finiti i messaggi pendenti
    ret = send_string(socket, DONE_SHOW);
    if (ret < 0) // Errore
        return;

    // Se c'erano dei messaggi pendenti, comunico al mittente dei messaggi la ricezione da parte del destinatario
    if (none_sent == 0)
        notify_reception(mittente, esecutore);
}

/*
 * Cerca in 'line' (riga del file contenente i messaggi pendenti) le informazioni sui
 * messaggi pendenti inviati da 'mittente'. Se li trova, aggiorna il timestamp del messaggio
 * più recente e il contatore dei messaggi pendenti, altrimenti crea una nuova entry.
 * 'line' non viene modificata: l'aggiornamento viene riflettuto in 'result'.
 */
void update_pendent_user_list(char* line, char* mittente, char* result) {
    int k, i = 0;
    char* start;
    char utente[USERNAME_LEN];
    char appoggio[MAX_LINE_LEN];
    char appoggio_numero[MAX_LINE_LEN];
    char appoggio_timestamp[TIMESTAMP_LEN];
    char old_line[MAX_LINE_LEN];

    strcpy(utente, ":");
    strcat(utente, mittente);
    strcat(utente, ":");
    start = strstr(line, utente);

    // Se ci sono già dei messaggi pendenti da 'mittente', aggiorno solo i campi
    if (start != NULL) {
        // 'appoggio' contiene i campi relativi ai messaggi pendenti inviati da 'mittente'
        strcpy(appoggio, start);

        // Recupero la parte iniziale della riga
        strcpy(old_line, line);
        old_line[strlen(line) - strlen(start) + 1] = '\0';

        // Quando trovo ':' dopo il mittente avrò finito di leggere il contatore dei messaggi pendenti
        for (k = strlen(mittente) + 2; appoggio[k] != ':'; k++) {
            appoggio_numero[i] = appoggio[k];
            i++;
        }
        appoggio_numero[i] = '\0';

        k++; // Aumento k (mi sono fermato su ':' nello scorrimento precedente)

        // Quando trovo ':' dopo il contatore avrò finito di leggere il timestamp del messaggio pendente più recente
        for (; appoggio[k] != ':'; k++) {}

        /* Fine campi relativi a 'mittente' */

        // Converto da stringhe a interi
        sprintf(appoggio_timestamp, "%d", (int) time(NULL)); // Timestamp corrente
        sprintf(appoggio_numero, "%d", atoi(appoggio_numero) + 1);
        fflush(stdout);

        // Ricompongo la riga con i campi aggiornati
        strcpy(result, old_line); // Parte iniziale della riga
        strcat(result, mittente);
        strcat(result, ":");
        strcat(result, appoggio_numero);
        strcat(result, ":");
        strcat(result, appoggio_timestamp);
        strcat(result, &appoggio[k]); // Parte rimanente della riga
    } else {
        remove_new_line(line); // Sostituisco il carattere new-line (\n) con il terminatore di stringa (\0)

        // Aggiungo i campi in fondo alla lista
        strcpy(result, line); // Riga originale
        strcat(result, mittente);
        strcat(result, ":1:"); // 1 solo messaggio pendente
        sprintf(appoggio_numero, "%d:", (int) time(NULL)); // Timestamp corrente
        strcat(result, appoggio_numero);
    }
}

/*
 * Registra nel file dei messaggi pendenti un nuovo messaggio inviato da 'mittente' per 'destinatario'.
 * In particolare, se ci sono già dei messaggi pendenti per 'destinatario' da 'mittente',
 * aggiorna il timestamp del messaggio più recente e il contatore dei messaggi pendenti,
 * altrimenti crea un'entry nel file.
 */
void new_pending_message(char* destinatario, char* mittente) {
    char tmp_file_path[PATH_MAX]; // File temporaneo
    FILE* hanging_list; // File contenente i messaggi temporanei
    FILE* hanging_tmp; // File temporaneo
    char line[MAX_LINE_LEN]; // Riga letta dal file
    char out[MAX_LINE_LEN]; // Riga da scrivere sul file
    int i = 0, j = 0;

    // Apro il file dei messaggi pendenti in lettura
    hanging_list = open_or_create(OFFLINE_MSG_FILE, "r");
    if (hanging_list == NULL)
        return; // Impossibile accedere al file

    // File di appoggio temporaneo
    strcpy(tmp_file_path, OFFLINE_MSG_FILE);
    strcat(tmp_file_path, "_tmp.txt");

    // Apro il file temporaneo in scrittura
    hanging_tmp = open_file(tmp_file_path, "w");
    if (hanging_tmp == NULL) { // Impossibile accedere al file
        if (fclose(hanging_list) != 0)
            fprintf(stderr, "Errore durante la chiusura del file di log della chat '%s' : %s\n", OFFLINE_MSG_FILE,
                    strerror(errno));

        return;
    }

    // Scorro il file finché non trovo il interlocutore
    for (;;) {
        if (fgets(line, MAX_LINE_LEN, hanging_list) == NULL)
            break; // Fine file

        remove_new_line(line); // Sostituisco il carattere new-line (\n) con il terminatore di stringa (\0)

        // Controllo la riga per verificare se sono a quella di 'destinatario'
        if (strcmp(line, destinatario) == 0 && i % 2 == 0) {
            /* La prossima riga contiene il campi da aggiornare */

            fprintf(hanging_tmp, "%s\n", line); // Lascio la riga inalterata
            j = 1;
        } else if (j == 1) {
            // Registra nuovo messaggio pendente
            update_pendent_user_list(line, mittente, out);
            fprintf(hanging_tmp, "%s\n", out);
            j--;
        } else
            fprintf(hanging_tmp, "%s\n", line); // Lascio la riga inalterata

        i++;
    }

    if (fclose(hanging_list) != 0)
        fprintf(stderr, "Errore durante la chiusura del file dei messaggi pendenti '%s' : %s\n", OFFLINE_MSG_FILE,
                strerror(errno));
    if (fclose(hanging_tmp) != 0)
        fprintf(stderr, "Errore durante la chiusura del file temporaneo '%s' : %s\n", tmp_file_path, strerror(errno));

    // Elimino il vecchio file di log e rinomino quello temporaneo (che prenderà il posto del vecchio)
    if (remove(OFFLINE_MSG_FILE) == -1)
        perror("Errore in new_pending_message() durante la cancellazione del vecchio file dei messaggi pendenti");
    if (rename(tmp_file_path, OFFLINE_MSG_FILE) == -1)
        perror("Errore mentre si tentava di rinominare il file di log temporaneo");
}

/*
 * Aggiunge un messaggio al log della chat tra 'mittente' e 'destinatario'
 */
void write_to_chat_log(char* mittente, char* destinatario, char* messaggio) {
    char path[PATH_MAX]; // Path del file di log della chat
    FILE* log; // File contenente lo storico della chat
    char appoggio[MAX_LINE_LEN]; // Conterrà la nuova riga da scrivere sul file di log

    /*
     * Se il file non viene trovato, provo a scambiare l'ordine degli username nel nome del file.
     * Ad esempio, il file di log può essere pippo-pluto.txt ma anche pluto-pippo.txt.
     */
    get_chat_log_path(mittente, destinatario, path);
    if (is_file_existing(path) == 0) // Il file di log non esiste
        get_chat_log_path(destinatario, mittente, path);
    log = open_or_create(path, "a"); // Apro il file in modalità append
    if (log == NULL)
        return; // Impossibile accedere al file

    // Contrassegno i messaggi con '*' poiché se arrivo qui l'utente è offline (altrimenti la gestirebbe il client)
    strcpy(appoggio, mittente);
    strcat(appoggio, ": ");
    strcat(appoggio, messaggio);
    strcat(appoggio, " ");
    strcat(appoggio, UNREAD_MARK);

    fprintf(log, "%s\n", appoggio);
    if (fclose(log) != 0)
        fprintf(stderr, "Errore durante la chiusura del log della chat '%s' : %s\n", path, strerror(errno));
}

/*
 * Invocata quando un client invia un messaggio di una chat al server (poiché il destinatario è offline).
 * Si occupa di ricevere il destinatario e il messaggio dal client e di registrare il nuovo messaggio
 * sui file (chat e messaggi pendenti).
 */
void new_message(int socket) {
    int ret;
    char messaggio[MAX_MSG_LEN]; // Messaggio spedito
    char mittente[USERNAME_LEN]; // Mittente del messaggio
    char destinatario[USERNAME_LEN]; // Destinatario del messaggio

    // Ricevo il destinatario del messaggio
    ret = receive_string(socket, destinatario);
    if (ret <= 0) { // Errore o disconnessione del client
        if (ret == 0)
            client_disconnection(socket);
        return;
    }

    // Ricevo il messaggio
    ret = receive_string(socket, messaggio);
    if (ret <= 0) { // Errore o disconnessione del client
        if (ret == 0)
            client_disconnection(socket);
        return;
    }

    find_username_from_socket(socket, mittente);
    if (mittente[0] == '\0') {
        printf("Errore in new_message(): nessun utente associato al socket.\n");
        return;
    }

    #ifdef DEBUG
    printf("Nuovo messaggio di una chat inviato da '%s' per '%s': '%s'.\n", mittente, destinatario, messaggio);
    #endif

    // Registro un nuovo messaggio pendente per 'mittente'
    new_pending_message(destinatario, mittente);

    // Scrivo il messaggio (come non letto) nei log della chat tra 'mittente' e 'destinatario'
    write_to_chat_log(mittente, destinatario, messaggio);

    // Segnala il completamento della registrazione del messaggio sui file
    ret = send_string(socket, LOGGED_MSG);
    if (ret < 0) // Errore
        return;
}

/*
 * Invocata quando un utente viene aggiunto alla chat di gruppo.
 * Si occupa di fornire le porte di ascolto dei membri del gruppo.
 */
void new_chat_member(int socket) {
    int ret;
    char membro[USERNAME_LEN];
    struct record_registro* record;

    // Ricevo l'username del membro
    ret = receive_string(socket, membro);
    if (ret <= 0) { // Errore o disconnessione del client
        if (ret == 0)
            client_disconnection(socket);
        return;
    }

    // Invio la porta di ascolto
    record = find_user_in_register(membro);
    ret = send_integer(socket, record == 0 || record->logout_timestamp != 0 ? INVALID_SOCKET : record->port);
    if (ret < 0) // Errore
        return;
}

/*
 * Verifica che il comando inviato dal client esista e lo esegue
 */
void run_client_command(int socket, char* comando) {
    char username[USERNAME_LEN];

    if (strcmp(comando, SIGNUP) == 0)
        signup(socket);
    else if (strcmp(comando, LOGIN) == 0)
        in(socket);
    else if (strcmp(comando, HANGING_COMMAND) == 0) {
        find_username_from_socket(socket, username);
        if (username[0] != '\0')
            hanging(socket, username);
        else
            printf("Errore in run_client_command(): nessun utente associato al socket.\n");
    } else if (strcmp(comando, LOGOUT_COMMAND) == 0)
        client_disconnection(socket);
    else if (strcmp(comando, NEW_CHAT_COMMAND) == 0)
        chat(socket);
    else if (strcmp(comando, SHOW_COMMAND) == 0)
        show(socket);
    else if (strcmp(comando, OFFLINE_MESSAGE) == 0)
        new_message(socket);
    else if (strcmp(comando, START_GROUP_CHAT) == 0)
        group_chat(socket);
    else if (strcmp(comando, CLIENT_PORT_REQUEST) == 0)
        insert_into_group_chat(socket);
    else if (strcmp(comando, MEMBER_PORT_REQUEST) == 0)
        new_chat_member(socket);
}

int main(int argc, char** argv) {
    fd_set read_fds; // Set contenente i socket pronti lasciati dalla select()
    int fd_max; // Massimo socket ID
    int porta; // Porta del server
    int i, ret;
    char buffer[MAX_MSG_LEN];

    // Si usa la porta passata come parametro all'avvio o quella di default se non viene specificata
    if (argv[1] != NULL) {
        porta = strtol(argv[1], NULL, 10);
        if (porta < 1024) {
            printf("Le prime 1024 porte sono riservate\n");
            exit(1);
        }
    } else
        porta = DEFAULT_SERVER_PORT;

    // Creazione socket di ascolto (protocollo TCP)
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Errore durante la creazione del socket di ascolto");
        exit(1);
    }

    // Bind e listen
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(porta);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(server_socket, (struct sockaddr*) &server_addr, sizeof(server_addr));
    if (ret < 0) {
        perror("Errore durante la bind");
        exit(-1);
    }
    ret = listen(server_socket, QUEUE_LEN);
    if (ret < 0) {
        perror("Errore durante la listen");
        exit(-1);
    }

    printf("************ SERVER STARTED ************\n");

    // Inizializzo i set per I/O multiplexing
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_SET(server_socket, &master); // Monitoro il socket di ascolto
    FD_SET(0, &master); // Monitoro lo stdin
    fd_max = server_socket;

    // Stampa la lista di comandi disponibili
    print_auth_commands();
    printf(">");
    fflush(stdout);

    while (1) {
        read_fds = master; // Dopo la select() conterrà solo i socket pronti
        ret = select(fd_max + 1, &read_fds, NULL, NULL, NULL);
        if (ret == -1) {
            perror("Errore nella select()");
            continue; // Salto all'iterazione continua in assenza di errori fatali
        }

        // Cerco il/i socket pronto/i
        for (i = 0; i <= fd_max; i++) {
            if (!FD_ISSET(i, &read_fds))
                continue;

            if (i == 0) { // Input da tastiera
                fgets(buffer, MAX_COMMAND_LEN, stdin);

                // Valida il comando inserito e lo esegue
                run_server_command(buffer);

                printf(">");
                fflush(stdout);
            } else if (i == server_socket) { // Socket di ascolto: ricevuta richiesta di connessione
                len = sizeof(client_addr);
                new_sd = accept(server_socket, (struct sockaddr*) &client_addr, (socklen_t * ) & len);

                // Aggiorno il set dei socket
                FD_SET(new_sd, &master);
                if (new_sd > fd_max)
                    fd_max = new_sd;

                #ifdef DEBUG
                printf("Nuovo client connesso al server\n");
                printf(">");
                fflush(stdout);
                #endif
            } else { // Socket di comunicazione: ricevuto un comando dal client

                // Ricevo il comando eseguito dal client
                ret = receive_string(i, buffer);
                if (ret <= 0) { // Errore o disconnessione del client
                    if (ret == 0)
                        client_disconnection(i);
                    continue;
                }

                // Verifico ed eseguo il comando ricevuto
                run_client_command(i, buffer);
            }
        }
    }
}