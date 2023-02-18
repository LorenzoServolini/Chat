/***************************************************
 *                                                 *
 *         Funzioni di utilità per i file          *
 *                                                 *
 **************************************************/

#include "file.h"
#include "../costanti.h"
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

/*
 * Crea il path del file contenente la rubrica dell'utente 'username' e lo inserisce in 'path'
 */
void get_contact_list_path(char* username, char* path) {
    strcpy(path, CONTACT_LIST_FOLDER);
    strcat(path, username);
    strcat(path, ".txt");

    #ifdef DEBUG
    printf("Path della rubrica di %s: '%s'.\n", username, path);
    #endif
}

/*
 * Crea il path del file contenente i log della chat intercorsa tra 'utente1' e 'utente2' e lo inserisce in 'path'
 */
void get_chat_log_path(char* utente1, char* utente2, char* path) {
    strcpy(path, CHAT_LOG_FOLDER);
    strcat(path, utente1);
    strcat(path, "-");
    strcat(path, utente2);
    strcat(path, ".txt");

    #ifdef DEBUG
    printf("Path del log della chat tra %s e %s: '%s'.\n", utente1, utente2, path);
    #endif
}

/*
 * Crea il path del file 'filename' nella cartella dei file condivisi dell'utente 'username' e lo inserisce in 'path'
 */
void get_shared_file_path(char* username, char* filename, char* path) {
    strcpy(path, SHARED_FILE_FOLDER);
    strcat(path, username);
    strcat(path, "/");
    strcat(path, filename);

    #ifdef DEBUG
    printf("Path del file '%s' condiviso: '%s'.\n", filename, path);
    #endif
}

/*
 * Crea il path del file condiviso con 'destinatario' (username) e lo inserisce in 'path'
 */
void get_received_file_path(char* destinatario, char* path) {
    char timestamp[TIMESTAMP_LEN];

    strcpy(path, SHARED_FILE_FOLDER);
    strcat(path, destinatario);
    strcat(path, "/file_ricevuto_");
    sprintf(timestamp, "%d", (int) time(NULL));
    strcat(path, timestamp);

    #ifdef DEBUG
    printf("Path del file ricevuto: '%s'.\n", path);
    #endif
}

/*
 * Verifica se il file identificato dal percorso 'path' esiste.
 * Restituisce 0 in caso non esita, altrimenti 1.
 */
int is_file_existing(char* path) {
    FILE* file = fopen(path, "r");
    if (file != NULL) { // Il file esiste
        fclose(file);
        return 1;
    }
    return 0;
}

/*
 * Apre il file identificato dal percorso 'path' con la modalità 'mode'. Se non esiste, il file viene creato.
 * Restituisce il puntatore al file o NULL se non è possibile accedere al file.
 * NB: ricordarsi di chiudere il file una volta terminato il suo uso!
 */
FILE* open_or_create(char* path, char* mode) {
    FILE* file = fopen(path, mode);

    // Se il file non esiste o si è verificato un errore
    if (file == NULL) {

        // Provo a creare il file (eventualmente di nuovo, per esempio nel caso di mode="a")
        file = fopen(path, "a");

        #ifdef DEBUG
        printf("Tentativo di creazione del file '%s'.\n", path);
        #endif

        // Impossibile accedere al file
        if (file == NULL) {
            perror("Impossibile accedere al file");
            return NULL;
        }

        fclose(file);

        #ifdef DEBUG
        printf("File creato con successo.\n");
        #endif

        file = fopen(path, mode);
    }

    #ifdef DEBUG
    printf("File '%s' aperto con successo.\n", path);
    #endif

    return file;
}

/*
 * Apre il file identificato dal percorso 'path' con la modalità 'mode'.
 * Restituisce il puntatore al file o NULL il file non esiste/non è possibile accederci.
 * NB: ricordarsi di chiudere il file una volta terminato il suo uso!
 */
FILE* open_file(char* path, char* mode) {
    FILE* file = fopen(path, mode);

    // Se il file non esiste o si è verificato un errore
    if (file == NULL) {
        #ifdef DEBUG
        printf("Il file '%s' non esiste. Impossibile aprirlo.\n", path);
        #endif
        return NULL;
    }

    #ifdef DEBUG
    printf("File '%s' aperto con successo.\n", path);
    #endif
    return file;
}

/*
 * Crea un file vuoto al percorso specificato. Se il file esiste già, la funzione non fa niente.
 * Restituisce 0 in caso di successo, -1 in caso di errore.
 */
int create_empty_file(char* path) {
    FILE* file = open_or_create(path, "a");
    if (file == NULL)
        return -1;

    fclose(file);
    return 0;
}

/*
 * Crea la cartella al percorso specificato. Se la cartella esiste già, la funzione non fa niente.
 * Restituisce 0 in caso di successo, -1 in caso di errore.
 */
int create_directory(char* path) {
    #ifdef DEBUG
    printf("Tentativo di creazione della cartella '%s'.\n", path);
    #endif

    // Creo la cartella con tutti i permessi (R,W,X) per tutti gli utenti
    int ret = mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
    if (ret != 0) {
        if (errno == EEXIST) { // La cartella esisteva già
            #ifdef DEBUG
            printf("La cartella esiste già.\n");
            #endif
            return 0;
        }

        perror("Impossibile creare la cartella ");
        return -1;
    }

    #ifdef DEBUG
    printf("Cartella creata con successo.\n");
    #endif
    return ret;
}