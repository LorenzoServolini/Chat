/************************************************************
 *                                                          *
 *      Funzioni di utilità per lo scambio di messaggi      *
 *                    (send() e recv())                     *
 *                                                          *
 ************************************************************/

#include "messaggi.h"
#include "string.h"
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

/*
 * Invia una stringa sul socket specificato.
 * Restituisce 0 in caso di successo, un valore negativo in caso di errore.
 */
int send_string(int socket, char* string) {
    int len = strlen(string);
    uint16_t network_order_len = htons(len);
    int ret;
    char tmp[len]; // Serve per evitare di modificare la stringa passata come parametro
    char buffer[len + sizeof(uint16_t)]; // Buffer contenente la lunghezza della stringa e la stringa

    // Copio la stringa per non modificare quella passata come parametro
    strcpy(tmp, string);

    // Sostituisco il new-line con il terminatore di stringa, se presente
    remove_new_line(tmp);

    #ifdef DEBUG
    printf("Invio sul socket %d la stringa '%s', di lunghezza %d.\n", socket, tmp, len);
    #endif

    // Invio messaggio (contenente lunghezza della stringa e stringa stessa)
    memcpy(buffer, &network_order_len, sizeof(uint16_t));
    memcpy(&buffer[sizeof(uint16_t)], &tmp, len);
    ret = send(socket, (void*) buffer, len + sizeof(uint16_t), 0);
    if (ret < 0) {
        perror("Errore durante l'invio di una stringa");
        return ret;
    }

    return 0;
}

/*
 * Invia un intero sul socket specificato.
 * Restituisce 0 in caso di successo, un valore negativo in caso di errore.
 */
int send_integer(int socket, int intero) {
    int ret;
    uint16_t network_order_int = htons(intero); // Converto l'intero da host-order a network-order

    #ifdef DEBUG
    printf("Invio sul socket %d l'intero '%d'.\n", socket, intero);
    #endif

    // Invio l'intero
    ret = send(socket, (void*) &network_order_int, sizeof(uint16_t), 0);
    if (ret < 0) {
        perror("Errore durante l'invio di un intero.\n");
        return ret;
    }

    return 0;
}

/*
 * Invia 'count' bit in 'bits' sul socket specificato.
 * Restituisce 0 in caso di successo, un valore negativo in caso di errore.
 */
int send_bit(int socket, void* bits, int count) {
    int ret;
    uint16_t network_order_len = htons(count);
    char buffer[count + sizeof(uint16_t)]; // Buffer contenente la lunghezza deela sequenza di bit e la sequenza di bit

    #ifdef DEBUG
    printf("Invio %d bit sul socket numero %d.\n", count, socket);
    #endif

    // Invio messaggio (contenente il numero di bit e la sequenza di bit stessa)
    memcpy(buffer, &network_order_len, sizeof(uint16_t));
    memcpy(&buffer[sizeof(uint16_t)], bits, count);
    ret = send(socket, (void*) buffer, count + sizeof(uint16_t), 0);
    if (ret < 0) {
        perror("Errore durante l'invio di bit");
        return ret;
    }

    return 0;
}

/*
 * Aspetta di ricevere un intero sul socket specificato. Pone in 'received' il numero ottenuto.
 * Restituisce 1 in caso di successo, 0 in caso di disconnessione del socket e
 * un numero negativo in caso di errore.
 */
int receive_integer(int socket, int* received) {
    int ret, tmp;

    // Ricevo l'intero
    ret = recv(socket, (void*) &tmp, sizeof(uint16_t), 0);
    if (ret <= 0) {
        if (ret == -1)
            perror("Errore durante la ricezione di un intero");

        return ret;
    }
    *received = ntohs(tmp); // Converto da network-order ad host-order

    #ifdef DEBUG
    printf("Sul socket %d è stato ricevuto l'intero '%d'.\n", socket, *received);
    #endif

    return 1;
}

/*
 * Aspetta di ricevere una stringa sul socket specificato. Pone in 'received' la stringa ottenuta.
 * Restituisce 1 in caso di successo, 0 in caso di disconnessione del socket e
 * un numero negativo in caso di errore.
 */
int receive_string(int socket, char* received) {
    int ret, len;
    uint16_t network_order_len;

    // Prelevo la lunghezza della stringa
    ret = recv(socket, (void*) &network_order_len, sizeof(uint16_t), 0);
    if (ret <= 0) {
        if (ret == -1)
            perror("Errore durante il prelievo della lunghezza di una stringa");

        return ret;
    }
    len = ntohs(network_order_len);

    #ifdef DEBUG
    printf("Ricevuta sul socket %d la lunghezza %d.\n", socket, len);
    #endif

    // Prelevo la stringa
    ret = recv(socket, received, len, 0);
    if (ret <= 0) {
        if (ret == -1)
            perror("Errore durante la ricezione di una stringa");

        return ret;
    }
    received[len] = '\0'; // Aggiungo il terminatore di stringa

    #ifdef DEBUG
    printf("Stringa ricevuta: '%s'.\n", received);
    #endif

    return 1;
}

/*
 * Aspetta di ricevere una serie di bit sul socket specificato. Pone in 'received' i bit ottenuti.
 * Restituisce 1 in caso di successo, 0 in caso di disconnessione del socket e
 * un numero negativo in caso di errore.
 */
int receive_bit(int socket, void* received) {
    int ret, len;
    uint16_t network_order_len;

    // Prelevo la lunghezza della sequenza di bit
    ret = recv(socket, (void*) &network_order_len, sizeof(uint16_t), 0);
    if (ret <= 0) {
        if (ret == -1)
            perror("Errore durante il prelievo della lunghezza di una sequenza di bit");

        return ret;
    }
    len = ntohs(network_order_len);

    #ifdef DEBUG
    printf("Lunghezza prelevata: %d.\n", len);
    #endif

    // Prelevo la sequenza di bit
    ret = recv(socket, (void*) received, len, 0);
    if (ret <= 0) {
        if (ret == -1)
            perror("Errore durante il prelievo di una sequenza di bit");

        return ret;
    }

    #ifdef DEBUG
    printf("Bit ricevuti correttamente.\n");
    #endif

    return 1;
}