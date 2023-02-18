/**********************************************
 *                                            *
 *           Codice dei dispositivi           *
 *                                            *
 **********************************************/

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include "costanti.h"
#include "util/messaggi.h"
#include "util/string.h"
#include "util/file.h"
#include "util/time.h"

// Elenco di comandi eseguibili (solo) durante una chat
enum CHAT_COMMAND {
    CLOSE_CHAT, SHARE, ADD_PARTECIPANT, NEW_MESSAGE
};

int server_port; // Porta di ascolto del server
int server_socket; // Socket di ascolto con il server
int client_port; // Porta su cui il client è in ascolto
int logged = 0; // Indica se è stato eseguito il login o meno
char username[USERNAME_LEN]; // Username dell'utente autenticato
int in_chat = 0; // Indica se c'è una chat in corso o meno
int in_group_chat = 0; // Indica se c'è una chat di gruppo in corso
int socket_gruppo[GROUP_SIZE]; // Contiene i socket di tutti i peer che partecipano alla chat
int peer_number = 0; // Numero di utenti nella chat di gruppo
char chat_users[GROUP_SIZE][USERNAME_LEN]; // Username degli utenti attualmente nella chat
int destinatario_offline = 0; // 1 quando il interlocutore è offline, altrimenti 0
int server_offline = 0; // 1 quando il server è offline, 0 se online
fd_set master; // Elenco di socket monitorati
int fd_max; // Numero di socket massimo

/*
 * Crea tutte le cartelle necessarie al funzionamento del device
 */
void create_folders(void) {
    int ret;

    ret = create_directory(SHARED_FILE_FOLDER);
    if (ret == -1)
        exit(1);

    ret = create_directory(CONTACT_LIST_FOLDER);
    if (ret == -1)
        exit(1);

    ret = create_directory(CHAT_LOG_FOLDER);
    if (ret == -1)
        exit(1);
}

/*
 * Stampa tutti i comandi disponibili
 */
void print_all_commands(void) {
    printf("*************************************************\n");
    printf("COMANDI DISPONIBILI:\n");
    printf("-> hanging: mostra il numero di messaggi pendenti\n");
    printf("-> show 'username': mostra i messaggi pendenti da 'username'\n");
    printf("-> chat 'username': avvia una chat con 'username'\n");
    printf("-> share 'file-name': invia 'file-name' ai device con cui si sta chattando\n");
    printf("-> out: disconnessione dal server\n");
    printf("*************************************************\n");
}

/*
 * Stampa la lista dei comandi per l'autenticazione
 */
void print_auth_commands(void) {
    printf("COMANDI DISPONIBILI:\n");
    printf("-> signup <username> <password/passphrase>: consente di registrarsi al servizio\n");
    printf("-> in <username> <password/passphrase>: consente di effettuare il login\n");
    printf("*************************************************\n");
}

/*
 * Ripulisce il terminale
 * Fonte: https://www.geeksforgeeks.org/clear-console-c-language/
 */
void clear_shell_screen(void) {
    printf("\e[1;1H\e[2J");
}

/*
 * Elimina visivamente la riga corrente dal terminale
 * Fonte: https://stackoverflow.com/a/1508589
 */
void clear_shell_line(void) {
    printf("\33[2K\r");
}

/*
 * Gestisce la disconnessione di 'socket' (recv() ha restituito 0)
 */
void socket_disconnection(int socket) {
    int k;

    close(socket);
    FD_CLR(socket, &master);

    if (socket == server_socket) { // Si è disconnesso il server
        printf("Server disconnesso. ");

        /*
         * Se non si è in una chat e il server va offline il device non può più fare niente
         * e quindi viene terminato.
         * Se invece è in una chat ma il destinatario è offline i messaggi inviati vengono
         * mandati al server che li registra. Analogamente quindi, se il server va offline,
         * il device non può più fare nulla e quindi viene chiuso.
         */
        if (in_chat == 0 || (in_chat == 1 && destinatario_offline == 1)) {
            printf("\n");
            exit(0);
        } else {
            printf("Quando chiuderai questa chat il device terminerà.\n");
            server_offline = 1;
        }
    } else { // Si è disconnesso un peer
        // Controllo se l'utente che si è disconnesso è membro della chat
        for (k = 0; k < peer_number; k++) {
            if (socket != socket_gruppo[k])
                continue;

            // Se la chat è 1-to-1
            if (in_group_chat == 0) {
                destinatario_offline = 1;

                /*
                 * Imposto il socket per lo scambio dei messaggi a quello del server
                 * (tutti i messaggi verranno adesso inviati a lui che li memorizzerà)
                 */
                socket_gruppo[0] = server_socket;
            } else { // Se la chat è di gruppo
                // Invio al server (che li memorizzerà) i messaggi destinati all'utente che si è disconnesso
                socket_gruppo[k] = server_socket;
            }
            break;
        }
    }
}

/*
 * Recupera l'username e la password dal comando ('signup' o 'in') inserito nel terminale.
 * Se trovate, verrà messo nei parametri 'username' e 'password' le credenziali. Altrimenti conterranno solo '\0'.
 */
void get_username_password_in_command(char* comando, char username[], char password[]) {
    char* user, * pass;

    /*
     * Divide la stringa in frammenti per ogni spazio trovato. Il 1° spazio indica dove iniziano i
     * parametri (username e password), il 2° dove finisce l'username e inizia la password. Sono consentite passphrase.
     * Fonte: https://stackoverflow.com/a/2523494
     *
     * Come specificato nella documentazione di strtok(), il terminatore di stringa è aggiunto
     * automaticamente al termine di ogni frammento (https://stackoverflow.com/q/17480576).
     */
    strtok(comando, " ");
    user = strtok(NULL, " ");
    pass = strtok(NULL, "\n"); // Arriva fino a fine stringa così da consentire passphrase

    // Se l'utente non ha specificato l'username o la password
    if (user == NULL || pass == NULL) {
        username[0] = '\0';
        password[0] = '\0';
        printf(user == NULL ? "Non hai inserito l'username!\n" : "Non hai inserito la password!\n");
        return;
    }

    strcpy(username, user);
    strcpy(password, pass);

    #ifdef DEBUG
    printf("username: '%s', password: '%s'\n", username, password);
    #endif
}

/*
 * Invia al server le credenziali per il login/signup.
 * 'operazione' indica se deve essere eseguito il login o la registrazione e 'password' è la password.
 */
void authenticate_to_server(char* operazione, char* password) {
    int ret;
    char buffer[MAX_MSG_LEN];

    // Invio il comando
    ret = send_string(server_socket, operazione);
    if (ret < 0)
        exit(1);

    // Invio l'username
    ret = send_string(server_socket, username);
    if (ret < 0)
        exit(1);

    // Invio la password
    ret = send_string(server_socket, password);
    if (ret < 0)
        exit(1);

    // Invio la porta del client
    ret = send_integer(server_socket, client_port);
    if (ret < 0)
        exit(1);

    // Ottengo l'esito dell'operazione inviato dal server
    ret = receive_string(server_socket, buffer);
    if (ret == 0) { // Disconnessione del server
        socket_disconnection(server_socket);

        /*
         * Se arriviamo qui non abbiamo chat aperte. Se il server si disconnette
         * il client non può fare niente, quindi si termina.
         */
        exit(0);
    }
    if (ret < 0) // Errore
        return;

    // L'utente si vuole registrare
    if (strcmp(operazione, SIGNUP) == 0) {
        printf("Registrazione in corso...\n");

        // Verifico la risposta ottenuta dal server riguardo la validità dell'username
        if (strcmp(buffer, ALREADY_EXISTING_USERNAME) == 0) {
            printf("Registrazione negata: esiste già un utente con quell'username.\n");
            return;
        } else if (strcmp(buffer, SIGNED_UP) == 0) {
            printf("Registrazione eseguita! Esegui il login per usare il tuo account.\n");
            print_auth_commands();
        }
    } else { // L'utente vuole eseguire il login
        printf("Login in corso...\n");

        // Verifico la risposta ottenuta dal server riguardo la correttezza delle credenziali
        if (strcmp(buffer, UNKNOWN_USER) == 0) {
            printf("Username non valido: non esiste un utente con questo username!\n");
            return;
        } else if (strcmp(buffer, WRONG_PASSWORD) == 0) {
            printf("Password non corretta.\n");
            return;
        } else if (strcmp(buffer, AUTHENTICATED) == 0) {
            printf("Login eseguito!\n");
            logged = 1;
            print_all_commands();
        }
    }

    /* L'utente si è autenticato con successo */

    // Crea la cartella personale dell'utente, utile per il corretto funzionamento del device
    strcpy(buffer, SHARED_FILE_FOLDER);
    strcat(buffer, username);
    ret = create_directory(buffer);
    if (ret == -1)
        exit(1);
}

/*
 * Comando 'signup': registra un nuovo utente.
 * Il parametro è il comando digitato dall'utente nel terminale.
 */
void signup(char* comando) {
    char password[PASSWORD_LEN];

    // Recupera l'username e la password dal comando inserito nel terminale
    get_username_password_in_command(comando, username, password);

    // Se i parametri sono validi, si procede con la registrazione
    if (strlen(username) != 0 && strlen(password) != 0)
        authenticate_to_server(SIGNUP, password);
}

/*
 * Comando 'in': esegue il login dell'utente.
 * Il parametro è il comando digitato dall'utente nel terminale.
 */
void in(char* comando) {
    char password[PASSWORD_LEN];

    // Recupera l'username e la password dal comando inserito nel terminale
    get_username_password_in_command(comando, username, password);

    // Se i parametri sono validi, si procede con il login
    if (strlen(username) != 0 && strlen(password) != 0)
        authenticate_to_server(LOGIN, password);
}

/*
 * Aspetta che l'utente si registri (comando 'signup') o esegua il login (comando 'in')
 */
void wait_for_login(void) {
    char buffer[MAX_COMMAND_LEN];

    do {
        printf(">");

        // Non si può usare scanf perchè questa spezza l'input in più stringhe secondo gli spazi
        fgets(buffer, MAX_COMMAND_LEN, stdin);

        if (strncmp(buffer, "signup ", 7) == 0) // Registrazione
            signup(buffer);
        else if (strncmp("in ", buffer, 3) == 0) // Login
            in(buffer);
        else {
            printf("Comando non valido.\n");
            print_auth_commands();
        }
    } while (logged == 0);
}

/*
 * Stampa lo storico della chat con 'interlocutore'
 */
void print_chat_history(char* interlocutore) {
    FILE* log; // File contenente i log della chat
    char path[PATH_MAX]; // Path del file contenente lo storico della chat
    char line[MAX_LINE_LEN]; // Riga letta dal file

    clear_shell_screen();

    if (destinatario_offline == 1)
        printf("%s non è online: i messaggi inviati adesso saranno recapitati al server.\n", interlocutore);
    else
        printf("%s è online.\n", interlocutore);

    /*
     * Se il file non viene trovato, provo a scambiare l'ordine degli username nel nome del file.
     * Ad esempio, il file di log può essere pippo-pluto.txt ma anche pluto-pippo.txt.
     */
    get_chat_log_path(interlocutore, username, path);
    if (is_file_existing(path) == 0) { // Il file di log non esiste
        get_chat_log_path(username, interlocutore, path);

        // Se non lo trovo di nuovo significa che non c'è stata alcuna chat tra i due utenti
        if (is_file_existing(path) == 0) {
            #ifdef DEBUG
            printf("Nessuna chat tra '%s' e '%s'. Il file di log verrà creato adesso.\n", username, interlocutore);
            #endif
        }
    }
    log = open_or_create(path, "r");
    if (log == NULL)
        return; // Impossibile accedere al file

    // Stampo lo storico della chat
    printf("--------------------------------\n");
    printf("Storico conversazione con '%s':\n", interlocutore);
    for (;;) {
        if (fgets(line, MAX_LINE_LEN, log) == NULL)
            break; // File terminato

        printf("%s", line);
    }
    printf("--------------------------------\n");

    if (fclose(log) != 0)
        fprintf(stderr, "Errore durante la chiusura del log della chat tra '%s' e '%s' : %s\n", username, interlocutore,
                strerror(errno));
}

/*
 * Stampa l'elenco degli utenti che fanno parte della chat
 */
void print_users_in_chat(void) {
    int k;

    // Chat di gruppo
    if (in_group_chat == 1)
        printf("Chat di gruppo con: \n");
    else if (in_chat == 1) // Chat 1-to-1
        printf("In chat con: \n");
    else { // Nessuna chat attiva
        printf("Non hai avviato alcuna chat!\n");
        return;
    }

    // Stampo tutti i membri della chat
    for (k = 0; strcmp(chat_users[k], "\0") != 0; k++)
        printf("- %s\n", chat_users[k]);
}

/*
 * Recupera il parametro di un comando inserito nel terminale e lo inserisce in 'parametro'.
 * Se il parametro non è valido, 'parametro' conterrà solo '\0'.
 */
void get_first_command_parameter(char* comando, char* parametro) {
    get_first_word_after_space(comando, parametro); // Recupero il parametro dentro il comando

    // Verifico che sia stato inserito il parametro
    if (parametro[0] == '\0')
        printf("Parametro non valido.\n");

    #ifdef DEBUG
    printf("Dal comando '%s' è stato ottenuto il parametro '%s'.\n", comando, parametro);
    #endif
}

/*
 * Invia il file (identificato da 'path') a tutti i membri della chat
 */
void share(char* path) {
    char buffer[FILE_MSG_SIZE];
    int ret, k;
    FILE* file; // File condiviso
    size_t byte_letti; // Numero di byte letti dal file da condividere

    // Verifico che l'interlocutore sia online
    if (destinatario_offline == 1) {
        printf("La condivisione di file è disponibile solo tra utenti online.\n");
        return;
    }

    clear_shell_line();

    // Verifico se il file che si vuole condividere esiste
    if (is_file_existing(path) == 0) {
        printf("Il file che vuoi condividere non esiste!\n");
        return;
    }

    printf("Invio il file a %d utente/i...\n", peer_number);

    // Invio il file a tutti i membri della chat
    for (k = 0; k < peer_number; k++) {

        // Invio il comando di condivisione file
        ret = send_string(socket_gruppo[k], SHARING_FILE);
        if (ret < 0) // Errore
            return;

        // Aspetto di ricevere l'ACK
        ret = receive_string(socket_gruppo[k], buffer);
        if (ret <= 0) { // Errore o disconnessione di un peer
            if (ret == 0)
                socket_disconnection(socket_gruppo[k]);
            return;
        }
        if (strcmp(buffer, ACK_SHARE) != 0) {
            printf("Errore durante la comunicazione con l'interlocutore sul socket %d.\n", socket_gruppo[k]);
            return;
        }
    }

    // Leggo tutto il file e invio i byte letti a ogni peer
    file = open_file(path, "rb"); // Apro il file in lettura binaria ('rb' = 'read binary')
    while ((byte_letti = fread(buffer, 1, FILE_MSG_SIZE, file)) > 0) {
        for (k = 0; k < peer_number; k++) {
            ret = send_bit(socket_gruppo[k], buffer, byte_letti);
            if (ret < 0) { // Errore
                if (fclose(file) != 0)
                    fprintf(stderr, "Errore durante la chiusura del file condiviso '%s : %s\n", path, strerror(errno));
                return;
            }
        }
    }
    if (fclose(file) != 0)
        fprintf(stderr, "Errore durante la chiusura del file condiviso '%s : %s\n", path, strerror(errno));

    // Notifico a tutti peer che ho terminato l'invio del file
    for (k = 0; k < peer_number; k++) {
        ret = send_string(socket_gruppo[k], DONE_SHARE);
        if (ret < 0) // Errore
            return;
    }

    printf("File inviato con successo.\n");
}

/*
 * Invia l'elenco degli utenti che possono essere aggiunti alla chat di gruppo e la crea
 * in base a quali username vuole aggiungere l'utente (dati in input da terminale)
 */
void add_member_to_chat(void) {
    char buffer[MAX_MSG_LEN];
    char appoggio[USERNAME_LEN + 3];
    int received; // Intero ricevuto su un socket
    int j = 0, k; // Indici per cicli for
    int ret, found = 0;
    char line[USERNAME_LEN]; // Linea letta nella rubrica
    char path[PATH_MAX]; // Path della rubrica
    FILE* rubrica;
    char utenti_inseribili[CONTACT_LIST_SIZE][USERNAME_LEN]; // Elenco degli utenti online che possono essere aggiunti alla chat
    struct sockaddr_in interlocutore_addr; // Utente che si vuole aggiungere nella chat
    int socket_p2p; // Socket peer-to-peer per comunicare con un altro dispositivo

    // Controllo se il singolo interlocutore attuale è online
    if (destinatario_offline == 1) {
        printf("Non è possibile avviare una chat di gruppo mentre il tuo interlocutore è offline.\n");
        return;
    }

    // Controllo se la rubrica esiste
    get_contact_list_path(username, path);
    if (is_file_existing(path) == 0) { // Rubrica inesistente
        printf("Errore durante l'apertura della rubrica di '%s'.\n", username);
        return;
    }

    // Invio il comando per avviare una chat di gruppo al server
    ret = send_string(server_socket, START_GROUP_CHAT);
    if (ret < 0) // Errore
        return;

    clear_shell_line();
    printf("Utenti online che possono essere aggiunti alla chat:\n");

    // Stampo l'elenco degli utenti in rubrica e online (solo questi possono essere aggiunti alla chat di gruppo)
    rubrica = open_file(path, "r");
    for (;;) {
        if (fgets(line, USERNAME_LEN, rubrica) == NULL)
            break; // Fine rubrica
        else {
            // Sostituisco il carattere new-line (\n) con il terminatore di stringa (\0)
            remove_new_line(line);

            // Non posso aggiungere me stesso alla chat
            if (strcmp(line, username) == 0)
                continue;

            // Se l'utente è già un membro della chat non può essere aggiunto di nuovo
            for (k = 0; k < peer_number + 1; k++) {
                if (strcmp(line, chat_users[k]) == 0)
                    found = 1;
                break;
            }
            if (found == 1) {
                found = 0;
                continue;
            }

            // Invio al server l'username che ci dirà se l'utente è online o meno
            ret = send_string(server_socket, line);
            if (ret < 0) { // Errore
                if (fclose(rubrica) != 0)
                    fprintf(stderr, "Errore durante la chiusura della rubrica di '%s' : %s\n", username,
                            strerror(errno));
                continue;
            }
            ret = receive_string(server_socket, buffer);
            if (ret == 0) { // Disconnessione del server
                socket_disconnection(server_socket);

                if (fclose(rubrica) != 0)
                    fprintf(stderr, "Errore durante la chiusura della rubrica di '%s' : %s\n", username,
                            strerror(errno));

                return;
            }
            if (ret < 0) { // Errore
                if (fclose(rubrica) != 0)
                    fprintf(stderr, "Errore durante la chiusura della rubrica di '%s' : %s\n", username,
                            strerror(errno));

                continue;
            }

            // Se l'utente è online, lo inserisco nell'elenco delle persone che possono essere aggiunte alla chat
            if (strcmp(buffer, USER_ONLINE) == 0) {
                strcpy(utenti_inseribili[j], line);
                printf("%d) %s\n", j + 1, utenti_inseribili[j]);
                j++;
            }
        }
    }

    // Segnalo al server la fine delle richieste per verificare se un utente è online o meno
    ret = send_string(server_socket, GROUP_CHAT_DONE);
    if (ret < 0) { // Errore
        if (fclose(rubrica) != 0)
            fprintf(stderr, "Errore durante la chiusura della rubrica di '%s' : %s\n", username, strerror(errno));

        return;
    }

    if (fclose(rubrica) != 0)
        fprintf(stderr, "Errore durante la chiusura della rubrica di '%s' : %s\n", username, strerror(errno));

    // Se non c'è nessun utente che può essere aggiunto
    if (j == 0) {
        printf("Nessuno :(\n");
        return;
    }

    // Attendo che l'utente inserisca l'username che vuole aggiungere alla chat
    printf("'\\a <username>' per aggiungere: ");
    fflush(stdout);

    // Leggo ciò che ha digitato l'utente
    fgets(buffer, USERNAME_LEN + 3, stdin);

    // Cerco nell'elenco degli utenti che possono essere aggiunti l'username che ha digitato l'utente
    for (k = 0; k < j; k++) {
        // Sostituisco il carattere new-line (\n) con il terminatore di stringa (\0)
        remove_new_line(buffer);

        // Verifico se l'username è nella lista (degli utenti che possono essere aggiunti in chat)
        strcpy(appoggio, "\\a ");
        strcat(appoggio, utenti_inseribili[k]);
        if (strcmp(buffer, appoggio) == 0) {
            found = 1;
            break;
        }
    }
    if (found == 0) {
        printf("Username non valido.\n");
        return;
    }

    /*
     * Per aggiungere l'utente alla chat devo mandargli una richiesta di inserimento (che può accettare o rifiutare).
     * Per fare ciò ho bisogno della porta su cui è in ascolto il client: contatto il server per richiedergliela.
     */
    ret = send_string(server_socket, CLIENT_PORT_REQUEST);
    if (ret < 0) // Errore
        return;
    ret = send_string(server_socket, utenti_inseribili[k]); // Invio l'username di cui si vuole conoscere la porta
    if (ret < 0) // Errore
        return;

    // Il server ci dice prima di tutto se l'utente è ancora online
    ret = receive_string(server_socket, buffer);
    if (ret <= 0) { // Errore o disconnessione del server
        if (ret == 0)
            socket_disconnection(server_socket);
        return;
    }
    if (strcmp(buffer, USER_ONLINE) != 0) { // L'utente è andato offline nel frattempo
        printf("L'utente è andato offline.\n");
        return;
    }

    // Ricevo la porta di ascolto del client
    ret = receive_integer(server_socket, &received);
    if (ret <= 0) { // Errore o disconnessione del server
        if (ret == 0)
            socket_disconnection(server_socket);
        return;
    }

    /*
     * Creo il socket peer-to-peer con il nuovo interlocutore: i
     * messaggi vengono inviati direttamente senza passare dal server
     */
    memset(&interlocutore_addr, 0, sizeof(interlocutore_addr));
    interlocutore_addr.sin_port = htons(received);
    interlocutore_addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &interlocutore_addr.sin_addr);
    socket_p2p = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_p2p == -1) {
        perror("Errore durante la creazione del socket peer-to-peer");
        return;
    }

    // Connessione al peer/interlocutore
    ret = connect(socket_p2p, (struct sockaddr*) &interlocutore_addr, sizeof(interlocutore_addr));
    if (ret == -1) {
        perror("Errore durante la connessione con l'altro peer");
        return;
    }

    // Invio l'invito a partecipare alla chat di gruppo e il mio username (richiedente)
    ret = send_string(socket_p2p, GROUP_CHAT_INVITE);
    if (ret < 0) // Errore
        return;
    ret = send_string(socket_p2p, username);
    if (ret < 0) // Errore
        return;

    printf("In attesa della risposta da '%s'...\n", utenti_inseribili[k]);

    // Ricevo la risposta all'invito (positiva o negativa)
    ret = receive_string(socket_p2p, buffer);
    if (ret <= 0) { // Errore o disconnessione del peer
        if (ret == 0)
            socket_disconnection(socket_p2p);
        return;
    }

    // Invito rifiutato
    if (strcmp(buffer, NO) == 0) {
        printf("Invito ad unirsi alla chat rifiutato :(\n");
        close(socket_p2p);
    } else { // Invito accettato
        // Aggiungo il socket peer-to-peer al set dei socket monitorati
        FD_SET(socket_p2p, &master);
        if (socket_p2p > fd_max)
            fd_max = socket_p2p;

        /*
         * Se ho appena creato la chat di gruppo, notifico all'altro
         * membro della chat che anch'io sono un membro della chat.
         */
        if (peer_number == 1 && destinatario_offline == 0) {
            ret = send_string(socket_gruppo[0], NEW_MEMBER);
            if (ret < 0) // Errore
                return;
            ret = send_string(socket_gruppo[0], username);
            if (ret < 0) // Errore
                return;
        }

        // Invio al nuovo utente gli username di tutti i membri della chat di gruppo
        for (j = 0; j < peer_number; j++) {
            ret = send_string(socket_p2p, chat_users[j]);
            if (ret < 0) // Errore
                return;
        }

        // Segnala la fine dell'invio dei membri della chat
        ret = send_string(socket_p2p, END_MEMBERS);
        if (ret < 0)
            return;

        printf("'%s' aggiunto alla chat!\n", utenti_inseribili[k]);

        //TODO: entrare in modalità chat (risolve diversi altri problemi)
        // Credo basti mettere in_chat = 1, pulire lo schermo e stampare lo storico

        // Aggiorno l'elenco dei partecipanti alla chat di gruppo
        strcpy(chat_users[peer_number], utenti_inseribili[k]);
        strcpy(chat_users[peer_number + 1], "\0");

        // Aggiorno l'elenco dei socket peer-to-peer dei partecipanti alla chat di gruppo
        socket_gruppo[peer_number] = socket_p2p;
        socket_gruppo[peer_number + 1] = INVALID_SOCKET;
        peer_number++;
        in_group_chat = 1;

        #ifdef DEBUG
        print_users_in_chat();
        #endif
    }
}

/*
 * Invia il messaggio 'msg' scritto in chat all'interlocutore (tramite 'socket_dest')
 */
void send_chat_message(int socket_dest, char* msg) {
    char buffer[MAX_MSG_LEN];
    int ret;

    // Se il interlocutore è offline, devo inviare i messaggi al server (che li memorizzerà)
    if (destinatario_offline == 1) {
        ret = send_string(socket_dest, OFFLINE_MESSAGE);
        if (ret < 0) // Errore
            return;
        ret = send_string(socket_dest, chat_users[0]);
        if (ret < 0) // Errore
            return;
    } else {
        // Invio il mio username al peer
        ret = send_string(socket_dest, username);
        if (ret < 0) // Errore
            return;
    }

    // Invio il messaggio
    ret = send_string(socket_dest, msg);
    if (ret < 0) // Errore
        return;

    /*
     * Per non accavallare richieste mi faccio inviare un messaggio quando
     * il server/i peer hanno finito di scrivere sul file che contiene
     * la cronologia della chat
     */
    ret = receive_string(socket_dest, buffer);
    if (ret <= 0) { // Errore o disconnessione del peer
        if (ret == 0)
            socket_disconnection(socket_dest);
        return;
    }
    if (strcmp(buffer, LOGGED_MSG) != 0) {
        printf("Errore durante la ricezione della risposta '%s' da un peer.\n'", LOGGED_MSG);
        return;
    }

    #ifdef DEBUG
    printf("Messaggio '%s' inviato con successo sul socket %d.\n", msg, socket_dest);
    #endif
}

/*
 * Invia il messaggio scritto in chat ('msg') a tutti i membri della chat di gruppo
 */
void send_group_message(char* msg) {
    char buffer[MAX_MSG_LEN];
    int i, ret;

    // Invia il messaggio a tutti i peer membri della chat di gruppo
    for (i = 0; i < peer_number; i++) {
        // Se l'interlocutore è offline, si invia il messaggio al server
        if (socket_gruppo[i] == server_socket) {
            ret = send_string(server_socket, OFFLINE_MESSAGE);
            if (ret < 0) // Errore
                continue;
            ret = send_string(server_socket, chat_users[i]);
            if (ret < 0) // Errore
                continue;
        } else {
            // Invio il mio username al peer
            ret = send_string(socket_gruppo[i], username);
            if (ret < 0) // Errore
                continue;
        }

        // Invio il messaggio vero e proprio
        ret = send_string(socket_gruppo[i], msg);
        if (ret < 0) // Errore
            continue;

        /*
         * Per non accavallare richieste mi faccio inviare un messaggio quando
         * il server/i peer hanno finito di scrivere sul file che contiene
         * la cronologia della chat
         */
        ret = receive_string(socket_gruppo[i], buffer);
        if (ret <= 0) { // Errore o disconnessione del peer
            if (ret == 0)
                socket_disconnection(socket_gruppo[i]);
            continue;
        }
        if (strcmp(buffer, LOGGED_MSG) != 0) {
            printf("Errore durante la ricezione della risposta '%s' da '%d'.\n", LOGGED_MSG, socket_gruppo[i]);
            continue;
        }
    }

    #ifdef DEBUG
    printf("Numero di peer nella chat di gruppo: %d.\n", peer_number);
    #endif
}

/*
 * Scrive sul file di log il login/logout dell'utente corrente.
 * Questa operazione viene eseguita dal client solo se il server è offline, altrimenti
 * normalmente se ne occuperebbe lui.
 */
void log_user_activity(void) {
    char timestamp[TIMESTAMP_LEN];
    FILE* utenti_log;

    // Apro il file in append
    utenti_log = open_or_create(ACTIVITY_LOG_FILE, "a");
    if (utenti_log == NULL)
        return; // Impossibile accedere al file

    // Registro l'ingresso dell'utente
    format_timestamp(time(NULL), timestamp, sizeof(timestamp));
    fprintf(utenti_log, "[%s] %s di %s (%s)\n", timestamp, "LOGOUT", username, "scritto da client");
    if (fclose(utenti_log) != 0)
        fprintf(stderr, "Errore durante la chiusura del file di log '%s' : %s\n", ACTIVITY_LOG_FILE, strerror(errno));

    #ifdef DEBUG
    printf("[%s] Username '%s' scritto su '%s'.", timestamp, username, ACTIVITY_LOG_FILE);
    #endif
}

/*
 * Si occupa di chiudere la chat in corso ed eventualmente, se il server è offline, terminare il client
 */
void close_chat(void) {
    int k;

    clear_shell_screen();
    printf("Chiusura chat in corso...\n");

    // Se il server è online
    if (server_offline == 0) {
        // Rimuovo tutti i membri nella chat
        for (k = 0; k < peer_number; k++) {
            strcpy(chat_users[k], "\0");
            socket_gruppo[k] = INVALID_SOCKET;
        }

        in_chat = 0;
        in_group_chat = 0;
        destinatario_offline = 0;
        peer_number = 0;
        printf("Sei uscito correttamente dalla chat.\n");
        print_all_commands();
        printf(">");
        fflush(stdout);
    } else { // Server offline
        printf("Il server è offline, perciò il client verrà chiuso.\n");

        // Aggiorno il timestamp di logout
        log_user_activity();

        // Termino
        exit(0);
    }
}

/*
 * Funzione invocata ogni volta che viene inviato un messaggio durante una chat.
 * Controlla se è un messaggio (da inviare su 'socket_dest') o un comando (per esempio
 * per creare una chat di gruppo) ed esegue le azioni necessarie.
 * Restituisce l'operazione che è stata inviata in chat.
 */
enum CHAT_COMMAND new_chat_message(char* msg) {
    char path[PATH_MAX];
    char nome_file[FILENAME_MAX];

    // Controllo se l'utente ha richiesto di uscire dalla chat
    if (strcmp(msg, "\\q") == 0) {
        close_chat();
        return CLOSE_CHAT;
    }

    // Comando per l'aggiunta di un nuovo partecipante alla chat (se necessario avvia una chat di gruppo)
    if (strcmp("\\u", msg) == 0) {
        add_member_to_chat();

        printf("%s>", username);
        fflush(stdout);

        return ADD_PARTECIPANT;
    }

    // Comando di share (eseguibile solo da una chat aperta)
    if (strncmp(msg, "share ", 6) == 0) {
        // Recupero il nome del file passato come parametro
        get_first_command_parameter(msg, nome_file);
        if (nome_file[0] == '\0')
            return SHARE; // Nome del file non valido

        // Recupero il path del file e lo invio
        get_shared_file_path(username, nome_file, path);
        share(path);

        printf("%s>", username);
        fflush(stdout);

        return SHARE;
    }

    /* Non ci sono altri comandi: è stato inserito un messaggio */

    // Se la chat in corso è una chat di gruppo
    if (in_group_chat == 1)
        send_group_message(msg); // Invio il messaggio a tutti i membri del gruppo
    else // Se la chat in corso non è una chat di gruppo
        send_chat_message(socket_gruppo[0], msg); // Invio il messaggio all'interlocutore

    // Pulisco lo schermo e ristampo la conversazione per un miglior effetto visivo
    clear_shell_screen();
    print_chat_history(chat_users[0]); // Stampo la chat con il membro che mi ha invitato

    printf("%s>", username);
    fflush(stdout);

    return NEW_MESSAGE;
}

/*
 * Aggiunge 'contatto' alla rubrica di 'user'
 */
void add_to_contact_list(char* user, char* contatto) {
    char path[PATH_MAX];
    FILE* rubrica;

    // Apro la rubrica in append
    get_contact_list_path(user, path);
    rubrica = open_or_create(path, "a");
    if (rubrica == NULL)
        return; // Impossibile accedere al file

    // Registro il nuovo contatto
    fprintf(rubrica, "%s\n", contatto);

    if (fclose(rubrica) != 0)
        fprintf(stderr, "Errore durante la chiusura della rubrica '%s' (di '%s') : %s\n", path, user, strerror(errno));

    #ifdef DEBUG
    printf("Username '%s' aggiunto con successo alla rubrica di '%s'.\n", contatto, user);
    #endif
}

/*
 * Comando 'hanging': stampa le informazioni sui messaggi pendenti
 */
void hanging(void) {
    char buffer[MAX_MSG_LEN];
    char timestamp[TIMESTAMP_LEN]; // Timestamp formattato
    int ret, i = 0;
    int none = 0; // Serve a verificare se ci sono messaggi pendenti

    printf("Verifico se ci sono messaggi pendenti...\n");

    // Invio il comando di hanging al server
    ret = send_string(server_socket, HANGING_COMMAND);
    if (ret < 0) // Errore
        return;

    // Ricevo le informazioni sui messaggi pendenti dal server, raggruppate per mittente
    for (;;) {
        /*
         * Se i == 0 -> si riceve l'username del mittente
         * Se i == 1 -> si riceve il numero di messaggi pendenti
         * Se i == 1 -> si riceve il timestamp del messaggio più recente
         */
        ret = receive_string(server_socket, buffer);
        if (ret == 0) { // Disconnessione del server
            socket_disconnection(server_socket);

            /*
             * Se arriviamo qui non abbiamo chat aperte. Se il server si disconnette
             * il client non può fare niente, quindi si termina.
             */
            exit(0);
        }
        if (ret < 0) // Errore
            return;

        if (strcmp(buffer, DONE_HANGING) == 0)
            break; // Fine messaggi pendenti

        if (i == 0) {
            printf("'%s' ti ha mandato ", buffer);
            none = 1;
            i++;
        } else if (i == 1) {
            printf("%s messaggio/i - ", buffer);
            i++;
        } else if (i == 2) {
            format_timestamp(atoi(buffer), timestamp, sizeof(timestamp));
            printf("%s\n", timestamp);
            i = 0;
        }
    }

    if (none == 0)
        printf("Nessun messaggio pendente trovato!\n");
}

/*
 * Cerca 'contatto' nella rubrica. Restituisce 0 in caso non ci sia, 1 se viene trovato.
 */
int find_user_in_contact_list(char* contatto) {
    char line[USERNAME_LEN];
    char path[PATH_MAX];
    FILE* rubrica;

    get_contact_list_path(username, path); // Ottengo il path della rubrica

    // Se la rubrica non esiste, sicuramente l'utente cercato non ci sarà
    if (is_file_existing(path) == 0)
        return 0;

    rubrica = open_file(path, "r");
    if (rubrica == NULL)
        return 0; // Impossibile accedere al file

    // Leggo la rubrica riga per riga alla ricerca dell'username
    for (;;) {
        if (fgets(line, USERNAME_LEN, rubrica) == NULL)
            return 0; // File terminato

        // Sostituisco il carattere new-line (\n) nella riga letta con il terminatore di stringa (\0)
        remove_new_line(line);

        if (strcmp(contatto, line) == 0)
            return 1; // Username trovato
    }
}

/*
 * Comando 'show': mostra i messaggi pendenti inviati da 'target_user' all'utente corrente
 */
void show(char* target_user) {
    int ret, num_messaggi_pendenti;
    char buffer[MAX_MSG_LEN];

    if (find_user_in_contact_list(target_user) == 0) {
        printf("L'utente non è in rubrica: non puoi fare una show su di lui.\n");
        return;
    }

    ret = send_string(server_socket, SHOW_COMMAND); // Invio al server il comando di show
    if (ret < 0)
        return;
    ret = send_string(server_socket, target_user); // Invio il mittente dei messaggi che si vogliono leggere
    if (ret < 0)
        return;

    // Ricevo i messaggi pendenti dal server
    for (;;) {
        ret = receive_string(server_socket, buffer);
        if (ret == 0) { // Disconnessione del server
            socket_disconnection(server_socket);

            /*
             * Se arriviamo qui non abbiamo chat aperte. Se il server si disconnette
             * il client non può fare niente, quindi si termina.
             */
            exit(0);
        }
        if (ret < 0) // Errore
            return;

        if (strcmp(buffer, DONE_SHOW) == 0)
            break; // Fine messaggi pendenti

        num_messaggi_pendenti++;
        printf("%s\n", buffer);
    }

    // Nessun messaggio pendente
    if (num_messaggi_pendenti == 0)
        printf("Mentre eri offline '%s' non ti ha inviato alcun messaggio :(\n", target_user);
}

/*
 * Avvia una chat con l'utente specificato nel parametro del comando 'comando'
 */
void chat(char* comando) {
    int ret;
    int peer_port; // Porta di ascolto del peer con cui si vuole avviare una chat
    char buffer[MAX_MSG_LEN];
    int socket_p2p; // Socket peer-to-peer per comunicare con un altro dispositivo
    struct sockaddr_in destinatario_addr; // Indirizzo del socket del peer con cui si vuole avviare una chat

    // Recupero l'username passato come parametro del comando
    get_first_command_parameter(comando, chat_users[0]);
    if (chat_users[0][0] == '\0')
        return; // Username non valido

    // Controllo se l'utente con cui si vuole parlare è in rubrica
    if (find_user_in_contact_list(chat_users[0]) == 0) {
        printf("Utente non trovato nella rubrica.\n");
        return;
    }

    printf("Avvio chat in corso...\n");

    // Si invia al server il comando che segnala la volontà di avviare una chat
    ret = send_string(server_socket, NEW_CHAT_COMMAND);
    if (ret < 0)
        return;
    // Si invia al server l'username dell'utente con cui si vuole avviare una chat
    ret = send_string(server_socket, chat_users[0]);
    if (ret < 0)
        return;

    // Il server ci dice se l'utente è online o meno
    ret = receive_string(server_socket, buffer);
    if (ret == 0) { // Disconnessione del server
        socket_disconnection(server_socket);

        /*
         * Se arriviamo qui non abbiamo chat aperte. Se il server si disconnette
         * il client non può fare niente, quindi si termina.
         */
        exit(0);
    }
    if (ret < 0) // Errore
        return;

    // Se l'interlocutore è offline, la conversazione avviene col server che memorizza nel log della chat i messaggi
    if (strcmp(buffer, USER_OFFLINE) == 0) { // Utente offline
        destinatario_offline = 1;

        // Imposto il socket per lo scambio dei messaggi a quello del server
        socket_p2p = server_socket;
    } else if (strcmp(buffer, USER_ONLINE) == 0) { // Utente online
        destinatario_offline = 0;

        // Se l'utente è online, mi viene mandata la porta su cui è in ascolto il device dell'interlocutore
        ret = receive_integer(server_socket, &peer_port);
        if (ret == 0) { // Disconnessione del server
            socket_disconnection(server_socket);

            /*
             * Se arriviamo qui non abbiamo chat aperte. Se il server si disconnette
             * il client non può fare niente, quindi si termina.
             */
            exit(0);
        }
        if (ret < 0) // Errore
            return;


        #ifdef DEBUG
        printf("Porta interlocutore: %d.\n", peer_port);
        #endif

        /*
         * Creo il socket peer-to-peer con il nuovo interlocutore: i
         * messaggi vengono inviati direttamente senza passare dal server
         */
        memset(&destinatario_addr, 0, sizeof(destinatario_addr));
        destinatario_addr.sin_port = htons(peer_port);
        destinatario_addr.sin_family = AF_INET;
        inet_pton(AF_INET, "127.0.0.1", &destinatario_addr.sin_addr);
        socket_p2p = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_p2p == -1) {
            perror("Errore durante la creazione del socket peer-to-peer");
            return;
        }

        // Connessione al peer/interlocutore
        ret = connect(socket_p2p, (struct sockaddr*) &destinatario_addr, sizeof(destinatario_addr));
        if (ret == -1) {
            perror("Errore nella connessione con l'altro peer");
            return;
        }

        // Aggiorno il set dei socket monitorati
        FD_SET(socket_p2p, &master);
        if (socket_p2p > fd_max)
            fd_max = socket_p2p;
    } else {
        printf("Errore durante la ricezione dal server dello status (online/offline) dell'utente '%s'.\n",
               chat_users[0]);
        return;
    }

    // Aggiungo il socket peer-to-peer alla lista dei socket che partecipano alla chat
    socket_gruppo[0] = socket_p2p;

    in_chat = 1; // L'utente è adesso in chat
    peer_number = 1; // Chat con 1 persona

    print_chat_history(chat_users[0]); // Stampo lo storico della chat con il nuovo interlocutore

    printf("%s", username);
    fflush(stdout);
}

/*
 * Comando 'out': disconnette e termina il device
 */
void out(void) {
    int ret, i;

    printf("Disconnessione dal server...\n");

    // Se il server è online, gli comunico il logout
    if (server_offline == 0) {
        ret = send_string(server_socket, LOGOUT_COMMAND);
        if (ret < 0) // Errore
            return;
    } else
        log_user_activity(); // Aggiorno il timestamp di logout (se fosse online lo farebbe il server)

    // Chiudo i socket peer-to-peer
    for (i = 0; i < peer_number; i++)
        if (socket_gruppo[i] != server_socket)
            socket_disconnection(socket_gruppo[i]);

    exit(0);
}

/*
 * Verifica che il comando inserito esista e lo esegue
 */
void run_command(char* comando) {
    char target[USERNAME_LEN];

    #ifdef DEBUG
    printf("Comando ricevuto: '%s'.\n", comando);
    #endif

    #ifdef DEBUG
    // Comando 'rubrica <username>': aggiunge l'utente alla rubrica
    if (strncmp("rubrica", comando, 7) == 0) {
        get_first_command_parameter(comando, target); // Recupero l'username specificato come parametro
        if (target[0] == '\0')
            return; // Username non valido

        add_to_contact_list(username, target);
        return;
    }
    #endif

    // Eseguo il comando
    if (strncmp("hanging", comando, 7) == 0)
        hanging();
    else if (strncmp("show ", comando, 5) == 0) {
        get_first_command_parameter(comando, target);
        if (target[0] == '\0')
            return; // Username non valido

        show(target);
    } else if (strncmp("chat ", comando, 5) == 0)
        chat(comando);
    else if (strncmp("share ", comando, 6) == 0)
        printf("Il comando può essere eseguito solo in una chat già in corso.\n");
    else if (strncmp("out", comando, 3) == 0)
        out();
    else {
        printf("Comando non valido.\n");
        print_all_commands();
    }
}

/*
 * Invocata ogni volta che viene aggiunto un membro alla chat.
 * Si occupa di registrare il nuovo membro della chat.
 */
void new_chat_member(int socket) {
    int ret;
    char tmp[USERNAME_LEN];

    // Ricevo l'username del nuovo membro
    ret = receive_string(socket, tmp);
    if (ret <= 0) { // Errore o disconnessione del peer (nuovo membro)
        if (ret == 0)
            socket_disconnection(socket);
        return;
    }

    // Se ho già una chat aperta (1-to-1) con il nuovo membro non devo fare niente
    if (strcmp(tmp, chat_users[0]) == 0)
        return;

    // Aggiungo il membro all'elenco dei partecipanti alla chat di gruppo
    strcpy(chat_users[peer_number], tmp);
    strcpy(chat_users[peer_number + 1], "\0");

    // Aggiorno l'elenco dei socket peer-to-peer dei partecipanti alla chat di gruppo
    socket_gruppo[peer_number] = socket;
    socket_gruppo[peer_number + 1] = INVALID_SOCKET;

    printf("'%s' è stato aggiunto alla chat di gruppo!\n", chat_users[peer_number]);

    peer_number++;
    in_group_chat = 1;

    print_users_in_chat();

    if (in_chat == 0)
        printf(">");
    else
        printf("%s>", username);
    fflush(stdout);
}

/*
 * Invocata quando il server notifica al mittente dei messaggi che il destinatario (inizialmente offline)
 * è tornato online e ha ricevuto i messaggi pendenti inviati in precedenza
 */
void pending_messages_sent(char* destinatario) {
    clear_shell_line();
    printf("Uno o più messaggi inviati a '%s' sono stati consegnati.\n", destinatario);

    if (in_chat == 1) // Se sono in chat
        printf("%s>", username);
    else
        printf(">");

    fflush(stdout);
}

/*
 * Si occupa di ricevere il file condiviso in chat da un peer
 */
void receive_file_shared(int socket) {
    int ret;
    char buffer[FILE_MSG_SIZE];
    char path[PATH_MAX]; // Path del file ricevuto
    FILE* file; // File ricevuto

    // Invio l'ACK per segnalare la ricezione del comando di condivisione file
    ret = send_string(socket, ACK_SHARE);
    if (ret < 0) // Errore
        return;

    get_received_file_path(username, path);
    file = open_or_create(path, "wb"); // 'wb' = 'write binary'

    // Ricevo il file condiviso
    for (;;) {
        ret = receive_bit(socket, buffer);
        if (ret <= 0) { // Errore o disconnessione del peer
            if (ret == 0)
                socket_disconnection(socket);

            if (fclose(file) != 0)
                fprintf(stderr, "Errore durante la chiusura del file ricevuto '%s' : %s\n", path, strerror(errno));
            return;
        }

        if (strncmp(buffer, DONE_SHARE, strlen(DONE_SHARE)) == 0)
            break; // Fine trasmissione file

        #ifdef DEBUG
        printf("Scrivo i bit sul file '%s'.\n", path);
        #endif

        fprintf(file, "%s", buffer); // Scrivo i byte ricevuti sul file
    }
    if (fclose(file) != 0)
        fprintf(stderr, "Errore durante la chiusura del file ricevuto '%s' : %s\n", path, strerror(errno));

    clear_shell_line();
    printf("** Nuovo file ricevuto **\n");
    if (in_chat == 0)
        printf(">");
    else
        printf("%s>", username);
    fflush(stdout);
}

/*
 * Invocata ogni volta che un utente esegue il login (il server ci invia una notifica). Controlla se l'utente
 * ora online fa parte della chat così che possa stabilirci una nuova connessione peer-to-peer (non passerò
 * più dal server).
 */
void now_online(void) {
    char buffer[USERNAME_LEN];
    int ret, peer_port, i;
    int socket_p2p; // Socket peer-to-peer per comunicare con un altro device
    struct sockaddr_in destinatario_addr; // Indirizzo del socket del peer con cui si vuole comunicare

    // Ricevo l'username dell'utente ora online
    ret = receive_string(server_socket, buffer);
    if (ret == 0) { // Disconnessione del server
        socket_disconnection(server_socket);

        /*
         * Se il server si disconnette e non abbiamo chat in corso o se abbiamo una chat con
         * un interlocutore offline, il client non può fare niente e dunque può terminare.
         */
        if (in_chat == 0 || (in_chat == 1 && destinatario_offline == 1))
            exit(0);

        return;
    }
    if (ret < 0) // Errore
        return;

    // Ricevo la porta su cui è in ascolto il device dell'utente ora online
    ret = receive_integer(server_socket, &peer_port);
    if (ret <= 0) { // Errore o disconnessione del server
        if (ret == 0)
            socket_disconnection(server_socket);
        return;
    }

    // Controllo se sono in chat con l'utente diventato online
    if ((in_chat == 1 || in_group_chat == 1) && destinatario_offline == 1) {
        for (i = 0; i < peer_number; i++) {
            if (strcmp(chat_users[i], buffer) == 0)
                break;
        }
        if (i == peer_number)
            return; // Corrispondenza non trovata

        /* Sono in chat con l'utente ora online */

        /*
         * Creo il socket peer-to-peer con il nuovo interlocutore: i
         * messaggi vengono inviati direttamente senza passare dal server
         */
        memset(&destinatario_addr, 0, sizeof(destinatario_addr));
        destinatario_addr.sin_port = htons(peer_port);
        destinatario_addr.sin_family = AF_INET;
        inet_pton(AF_INET, "127.0.0.1", &destinatario_addr.sin_addr);
        socket_p2p = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_p2p == -1) {
            perror("Errore durante la creazione del socket peer-to-peer");
            return;
        }

        // Connessione al peer/interlocutore
        ret = connect(socket_p2p, (struct sockaddr*) &destinatario_addr, sizeof(destinatario_addr));
        if (ret == -1) {
            perror("Errore nella connessione con l'altro peer");
            return;
        }

        // Aggiorno il set dei socket monitorati
        FD_SET(socket_p2p, &master);
        if (socket_p2p > fd_max)
            fd_max = socket_p2p;

        // Aggiungo il socket peer-to-peer alla lista dei socket che partecipano alla chat
        socket_gruppo[i] = socket_p2p;

        destinatario_offline = 0;
    }
}

/*
 * Aggiunge un messaggio al log della chat tra l'utente corrente ('username') e 'mittente'
 */
void write_to_chat_log(char* mittente, char* messaggio) {
    char path[PATH_MAX]; // Path del file di log della chat
    FILE* log; // File contenente lo storico della chat
    char appoggio[MAX_LINE_LEN]; // Conterrà la nuova riga da scrivere sul file di log

    /*
     * Se il file non viene trovato, provo a scambiare l'ordine degli username nel nome del file.
     * Ad esempio, il file di log può essere pippo-pluto.txt ma anche pluto-pippo.txt.
     */
    get_chat_log_path(mittente, username, path);
    if (is_file_existing(path) == 0) // Il file di log non esiste
        get_chat_log_path(username, mittente, path);
    log = open_or_create(path, "a"); // Apro il file in modalità append
    if (log == NULL)
        return; // Impossibile accedere al file

    // Contrassegno i messaggi con '**' poiché se arrivo qui sono online e li sto leggendo
    strcpy(appoggio, mittente);
    strcat(appoggio, ": ");
    strcat(appoggio, messaggio);
    strcat(appoggio, " ");
    strcat(appoggio, READ_MARK);

    fprintf(log, "%s\n", appoggio); // Scrivo sul file il messaggio
    if (fclose(log) != 0)
        fprintf(stderr, "Errore durante la chiusura del log della chat '%s' : %s\n", path, strerror(errno));
}

/*
 * Crea il socket di ascolto e aspetta che un socket nel set dei socket monitorati sia pronto
 */
void start_listening(void) {
    char mittente[USERNAME_LEN];
    char messaggio[MAX_MSG_LEN];
    char buffer[MAX_COMMAND_LEN];
    int porta; // Porta di ascolto di un peer ricevuta dal server
    int ret, len, i, k;
    int found = 0;
    int new_sd; // Contiene un nuovo socket creato
    struct sockaddr_in client_address; // Indirizzo (del socket) del client
    struct sockaddr_in mittente_address; // Indirizzo (del socket) del client che ci ha inviato una richieste di connessione
    int listen_socket; // Socket di ascolto per altri peer
    fd_set read_fds; // Set di socket di appoggio
    int socket_p2p; // Socket peer-to-peer per comunicare con un altro dispositivo
    struct sockaddr_in destinatario_addr; // Indirizzo di un peer

    // Creo il socket di ascolto (protocollo TCP)
    listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket == -1) {
        perror("Errore durante la creazione del socket di ascolto");
        exit(1);
    }

    // Bind e listen del socket di ascolto
    memset(&client_address, 0, sizeof(client_address));
    client_address.sin_family = AF_INET;
    client_address.sin_port = htons(client_port);
    client_address.sin_addr.s_addr = INADDR_ANY;
    ret = bind(listen_socket, (struct sockaddr*) &client_address, sizeof(client_address));
    if (ret == -1) {
        perror("Errore durante la bind");
        exit(1);
    }
    ret = listen(listen_socket, QUEUE_LEN);
    if (ret == -1) {
        perror("Errore durante la listen");
        exit(1);
    }

    // Inizializzo i set per I/O multiplexing
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_SET(listen_socket, &master);
    FD_SET(0, &master);
    FD_SET(server_socket, &master);
    fd_max = server_socket > listen_socket ? server_socket : listen_socket;

    printf(">");
    fflush(stdout);

    for (;;) {
        read_fds = master; // Dopo la select() conterrà solo i socket pronti
        ret = select(fd_max + 1, &read_fds, NULL, NULL, NULL);
        if (ret == -1) {
            perror("Errore durante la select");
            exit(1);
        }

        // Cerco il/i socket pronto/i
        for (i = 0; i <= fd_max; i++) {
            if (!FD_ISSET(i, &read_fds))
                continue; // Il socket i non è pronto

            if (i == listen_socket) { // Nuova connessione
                len = sizeof(mittente_address);
                new_sd = accept(listen_socket, (struct sockaddr*) &mittente_address, (socklen_t * ) & len);

                // Aggiorno il set dei socket monitorati
                FD_SET(new_sd, &master);
                if (new_sd > fd_max)
                    fd_max = new_sd;
            } else if (i == 0) { // Input da tastiera
                /*
                 * Leggo ciò che è stato inserito nel terminale: non si usa scanf
                 * perchè questa spezza l'input in più stringhe secondo gli spazi
                 */
                fgets(buffer, MAX_COMMAND_LEN, stdin);

                // Sostituisco il carattere di new-line (\n) con il terminatore di stringa (\0)
                remove_new_line(buffer);

                // Se c'è una chat in corso (1-to-1 o di gruppo non fa differenza)
                if (in_chat == 1) {
                    printf("%s>", username);
                    fflush(stdout);

                    new_chat_message(buffer);
                } else { // Non sono in una chat: è un comando "esterno" alle chat
                    run_command(buffer);
                    printf(">");
                    fflush(stdout);
                }
            } else { // Ho ricevuto un comando o un messaggio

                // Ricevo il comando o il mittente (se è un messaggio)
                ret = receive_string(i, buffer);
                if (ret == 0) { // Disconnessione di un peer/del server
                    socket_disconnection(i);
                    continue;
                }
                if (ret < 0) // Errore
                    continue;

                /* Verifico il comando/messaggio ricevuto */

                // Invito ad una chat di gruppo
                if (strcmp(buffer, GROUP_CHAT_INVITE) == 0) {
                    // Se sono già in chat (1-to-1 o di gruppo), rifiuto l'invito
                    if (in_chat == 1) {
                        ret = send_string(i, NO);
                        if (ret < 0) // Errore
                            continue;
                    }

                    // Ricevo l'username di chi mi ha invitato
                    ret = receive_string(i, mittente);
                    if (ret <= 0) { // Errore o disconnessione di un peer
                        socket_disconnection(i);
                        continue;
                    }

                    clear_shell_line();
                    printf("Ricevuto invito a partecipare ad una chat di gruppo da '%s'. Partecipare? [y/n]: ",
                           mittente);

                    // Leggo la risposta digitata sostituendo il carattere new-line (\n) con il terminatore di stringa (\0)
                    fgets(buffer, MAX_LINE_LEN, stdin);
                    remove_new_line(buffer);
                    if (equals_ignore_case(buffer, YES) == 0 && equals_ignore_case(buffer, NO) == 0) {
                        printf("Risposta non valida: invito rifiutato.\n");
                        ret = send_string(i, NO);
                        if (ret < 0) // Errore
                            continue;
                        continue;
                    }

                    // Invio la risposta all'invito
                    ret = send_string(i, buffer);
                    if (ret < 0)
                        continue;

                    printf(">");
                    fflush(stdout);

                    // Se l'invito viene rifiutato non c'è altro da fare
                    if (strcmp(buffer, NO) == 0)
                        continue;

                    // Entro nella chat di gruppo
                    socket_gruppo[peer_number] = i;
                    socket_gruppo[peer_number + 1] = INVALID_SOCKET;
                    strcpy(chat_users[peer_number], mittente);
                    strcpy(chat_users[peer_number + 1], "\0");
                    peer_number++;

                    // Ricevo tutti i membri della chat da chi mi ha inviato l'invito a partecipare alla chat
                    for (;;) {
                        ret = receive_string(i, buffer);
                        if (ret <= 0) { // Errore o disconnessione del peer
                            if (ret == 0)
                                socket_disconnection(i);
                            break;
                        }

                        // Se sono finiti i membri della chat
                        if (strcmp(buffer, END_MEMBERS) == 0) {
                            strcpy(chat_users[peer_number], "\0");
                            in_group_chat = 1;
                            in_chat = 1;
                            clear_shell_screen();
                            print_chat_history(mittente);
                            print_users_in_chat();
                            printf("%s>", username);
                            fflush(stdout);
                            break;
                        }

                        // Aggiungo l'utente alla lista dei membri della chat
                        strcpy(chat_users[peer_number], buffer);

                        /*
                         * Contatto il server per ricevere porta di ascolto di ogni membro della chat
                         * al fine di stabilirci una connessione peer-to-peer.
                         * Si invia il comando per richiedere la porta e l'username di cui si vuole
                         * conoscere la porta di ascolto.
                         */
                        ret = send_string(server_socket, MEMBER_PORT_REQUEST);
                        if (ret < 0) // Errore
                            break;
                        ret = send_string(server_socket, chat_users[peer_number]);
                        if (ret < 0) // Errore
                            break;

                        // Ricevo la porta di ascolto dell'utente dal server
                        ret = receive_integer(server_socket, &porta);
                        if (ret <= 0) { // Errore o disconnessione del server
                            socket_disconnection(server_socket);
                            break;
                        }
                        if (porta == INVALID_SOCKET) {
                            // Il membro è offline: invio i messaggi al server
                            socket_gruppo[peer_number] = server_socket;
                            peer_number++;
                            continue;
                        }

                        // Compilo con i dati dell'altro peer (membro)
                        memset(&destinatario_addr, 0, sizeof(destinatario_addr));
                        destinatario_addr.sin_port = htons(porta);
                        destinatario_addr.sin_family = AF_INET;
                        inet_pton(AF_INET, "127.0.0.1", &destinatario_addr.sin_addr);
                        socket_p2p = socket(AF_INET, SOCK_STREAM, 0);
                        if (socket_p2p == -1) {
                            perror("Errore durante la creazione di un socket peer-to-peer");
                            break;
                        }

                        // Connessione al peer (membro)
                        ret = connect(socket_p2p, (struct sockaddr*) &destinatario_addr, sizeof(destinatario_addr));
                        if (ret == -1) {
                            perror("Errore nella connessione con l'altro peer\n");
                            break;
                        }

                        // Aggiorno l'elenco dei socket peer-to-peer dei partecipanti alla chat di gruppo
                        socket_gruppo[peer_number] = socket_p2p;
                        peer_number++;

                        printf("Connessione con '%s' eseguita con successo.\n", chat_users[peer_number]);

                        // Notifico al membro che sono stato aggiunto alla chat di gruppo
                        ret = send_string(socket_p2p, NEW_MEMBER);
                        if (ret < 0) // Errore
                            break;
                        ret = send_string(socket_p2p, username);
                        if (ret < 0) // Errore
                            break;

                        // Aggiorno il set dei socket monitorati
                        FD_SET(socket_p2p, &master);
                        if (socket_p2p > fd_max)
                            fd_max = socket_p2p;
                    }
                } else if (strcmp(buffer, NEW_MEMBER) == 0) { // Nuovo membro aggiunto alla chat di gruppo
                    new_chat_member(i);
                    continue;
                } else if (strcmp(buffer, MESSAGES_SENT) == 0) { // Un utente ha ricevuto i messaggi pendenti
                    // Ricevo l'username del interlocutore a cui sono arrivati i messaggi pendenti
                    ret = receive_string(i, buffer);
                    if (ret == 0) { // Disconnessione del server
                        socket_disconnection(i);

                        /*
                         * Se il server si disconnette e non abbiamo chat in corso o se abbiamo una chat con
                         * un interlocutore offline, il client non può fare niente e dunque può terminare.
                         */
                        if (in_chat == 0 || (in_chat == 1 && destinatario_offline == 1))
                            exit(0);

                        continue;
                    }
                    if (ret < 0) // Errore
                        continue;

                    pending_messages_sent(buffer);
                    continue;
                } else if (strcmp(buffer, SHARING_FILE) == 0) { // Condivisione di un file
                    receive_file_shared(i);
                    continue;
                } else if (strcmp(buffer, NOW_ONLINE) == 0) { // Un utente ha eseguito il login (ed è ora online)
                    now_online();
                    continue;
                } else { // Messaggio in chat
                    strcpy(mittente, buffer); // Ho ricevuto il mittente del messaggio

                    // Ricevo il messaggio
                    ret = receive_string(i, messaggio);
                    if (ret <= 0) { // Errore o disconnessione del socket
                        if (ret == 0)
                            socket_disconnection(i);
                        continue;
                    }

                    // Se non sono in nessuna chat
                    if (in_chat == 0) {
                        printf("** Nuovo messaggio da '%s' **\n", mittente);

                        // Salvo il messaggio sul log della chat (se non sono nella solita)
                        write_to_chat_log(mittente, messaggio);

                        ret = send_string(i, LOGGED_MSG);
                        printf(">");
                        fflush(stdout);
                        if (ret < 0) // Errore
                            continue;
                    } else { // Sono già in una chat
                        // Controllo se chi mi ha inviato un messaggio è tra i membri della chat corrente
                        for (k = 0; k < peer_number; k++) {
                            if (strcmp(mittente, chat_users[k]) == 0) {
                                found = 1;
                                break;
                            }
                        }

                        clear_shell_line();

                        // Salvo il messaggio sul log della chat
                        write_to_chat_log(mittente, messaggio);

                        if (found == 1) // Sono in chat col mittente
                            print_chat_history(mittente);
                        else // Sono in una chat con altri utenti
                            printf("** Nuovo messaggio da '%s' **\n", mittente);

                        // Invio la conferma di avvenuta registrazione del messaggio sul file di log della chat
                        ret = send_string(i, LOGGED_MSG);
                        printf("%s>", username);
                        fflush(stdout);
                        if (ret < 0) // Errore
                            continue;
                    }
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    int ret;
    struct sockaddr_in server_addr; // Indirizzo (del socket) del server

    // Recupero il numero di porta (del client) specificato all'avvio
    if (argc > 1) {
        client_port = atoi(argv[1]);
        if (client_port < 1024) {
            printf("Le prime 1024 porte sono riservate.\n");
            exit(1);
        }
    } else {
        printf("La porta del client DEVE essere specificata all'avvio!\n");
        exit(1);
    }

    // Se il server è in ascolto su una porta diversa da quella di default, verrà passata come 2° parametro
    if (argc > 2) {
        server_port = atoi(argv[2]);
        if (server_port < 1024) {
            printf("Le prime 1024 porte sono riservate.\n");
            exit(1);
        }
    } else
        server_port = DEFAULT_SERVER_PORT; // Porta di default

    // Creazione socket per comunicare con il server
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Errore durante la creazione del socket per le comunicazioni con il server");
        exit(1);
    }

    // Settaggio indirizzo del server
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    // Connessione al server
    ret = connect(server_socket, (struct sockaddr*) &server_addr, sizeof(server_addr));
    if (ret < 0) {
        perror("Errore durante la connessione al server");
        exit(1);
    }

    printf("************ CONNESSO AL SERVER CON SUCCESSO ************\n");

    // Creo le cartelle, se non esistono, necessarie per il funzionamento del device
    create_folders();

    // Stampo i comandi disponibili
    print_auth_commands();

    /*
     * L'utente deve registrarsi (comando 'signup') o eseguire il login (comando 'in').
     * Finché non lo farà non potrà svolgere nessun'altra operazione.
     */
    wait_for_login();

    // Mi metto in ascolto di nuovi messaggi
    start_listening();

    return 0;
}