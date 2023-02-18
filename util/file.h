/***************************************************
 *                                                 *
 *         Funzioni di utilità per i file          *
 *                                                 *
 **************************************************/

#include <stdio.h>

/*
 * Crea il path del file contenente la rubrica dell'utente 'username' e lo inserisce in 'path'
 */
void get_contact_list_path(char* username, char* path);

/*
 * Crea il path del file contenente i log della chat intercorsa tra 'utente1' e 'utente2' e lo inserisce in 'path'
 */
void get_chat_log_path(char* utente1, char* utente2, char* path);

/*
 * Crea il path del file 'filename' nella cartella dei file condivisi dell'utente 'username' e lo inserisce in 'path'
 */
void get_shared_file_path(char* username, char* filename, char* path);

/*
 * Crea il path del file condiviso con 'destinatario' (username) e lo inserisce in 'path'
 */
void get_received_file_path(char* destinatario, char* path);

/*
 * Verifica se il file identificato dal percorso 'path' esiste.
 * Restituisce 0 in caso non esita, altrimenti 1.
 */
int is_file_existing(char* path);

/*
 * Apre il file identificato dal percorso 'path' con la modalità 'mode'. Se non esiste, il file viene creato.
 * Restituisce il puntatore al file o NULL se non è possibile accedere al file.
 * NB: ricordarsi di chiudere il file una volta terminato il suo uso!
 */
FILE* open_or_create(char* path, char* mode);

/*
 * Apre il file identificato dal percorso 'path' con la modalità 'mode'.
 * Restituisce il puntatore al file o NULL il file non esiste/non è possibile accederci.
 * NB: ricordarsi di chiudere il file una volta terminato il suo uso!
 */
FILE* open_file(char* path, char* mode);

/*
 * Crea un file vuoto al percorso specificato. Se il file esiste già, la funzione non fa niente.
 * Restituisce 0 in caso di successo, -1 in caso di errore.
 */
int create_empty_file(char* path);

/*
 * Crea la cartella al percorso specificato. Se la cartella esiste già, la funzione non fa niente.
 * Restituisce 0 in caso di successo, -1 in caso di errore.
 */
int create_directory(char* path);