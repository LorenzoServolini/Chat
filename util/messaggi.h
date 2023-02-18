/************************************************************
 *                                                          *
 *      Funzioni di utilità per lo scambio di messaggi      *
 *                    (send() e recv())                     *
 *                                                          *
 ************************************************************/

/*
 * Invia una stringa sul socket specificato.
 * Restituisce 0 in caso di successo, un valore negativo in caso di errore.
 */
int send_string(int socket, char* string);

/*
 * Invia un intero sul socket specificato.
 * Restituisce 0 in caso di successo, un valore negativo in caso di errore.
 */
int send_integer(int socket, int intero);

/*
 * Invia 'count' bit in 'bits' sul socket specificato.
 * Restituisce 0 in caso di successo, un valore negativo in caso di errore.
 */
int send_bit(int socket, void* bits, int count);

/*
 * Aspetta di ricevere un intero sul socket specificato. Pone in 'received' il numero ottenuto.
 * Restituisce 1 in caso di successo, 0 in caso di disconnessione del socket e
 * un numero negativo in caso di errore.
 */
int receive_integer(int socket, int* received);

/*
 * Aspetta di ricevere una stringa sul socket specificato. Pone in 'received' la stringa ottenuta.
 * Restituisce 1 in caso di successo, 0 in caso di disconnessione del socket e
 * un numero negativo in caso di errore.
 */
int receive_string(int socket, char received[]);

/*
 * Aspetta di ricevere una serie di bit sul socket specificato. Pone in 'received' i bit ottenuti.
 * Restituisce 1 in caso di successo, 0 in caso di disconnessione del socket e
 * un numero negativo in caso di errore.
 */
int receive_bit(int socket, void* received);