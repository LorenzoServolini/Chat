/***************************************************
 *                                                 *
 *       Funzioni di utilità per le stringhe       *
 *                                                 *
 **************************************************/

#include "string.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*
 * Sostituisce, se presente, il carattere di new-line (\n) con il terminatore di stringa (\0)
 */
void remove_new_line(char* string) {
    char* newline = strchr(string, '\n');
    if (newline != NULL)
        *newline = '\0'; // Sostituisco \n con \0 per terminare la stringa

    #ifdef DEBUG
    printf("Stringa senza new-line: '%s'.\n", string);
    #endif
}

/*
 * Recupera la prima parola dopo uno spazio e la inserisce in 'result'.
 * Conterrà solo il terminatore di stringa (\0) se non c'è nessuno spazio o nessuna parola dopo uno spazio.
 */
void get_first_word_after_space(char* string, char* result) {
    char* word;
    char tmp[strlen(string)];

    // Copio la stringa per non modificare quella passata come parametro
    strcpy(tmp, string);

    /*
     * Divide la stringa in frammenti per ogni spazio trovato. Fonte: https://stackoverflow.com/a/2523494
     *
     * Come specificato nella documentazione di strtok(), il terminatore di stringa è aggiunto
     * automaticamente al termine di ogni frammento (https://stackoverflow.com/q/17480576).
     */
    strtok(tmp, " ");
    word = strtok(NULL, " "); // Ottengo la prima parola

    if (word == NULL) // Se non ci sono spazi o non c'è una parola dopo lo spazio
        result[0] = '\0';
    else // Copia la parola in result
        strcpy(result, word);

    #ifdef DEBUG
    printf("Prima parola dopo lo spazio trovata: '%s'.\n", result);
    #endif
}

/*
 * Controlla se due le stringhe sono uguali ignorando le lettere maiuscole e minuscole.
 * Restituisce 1 se sono uguali, 0 se sono diverse.
 */
int equals_ignore_case(char* str1, char* str2) {
    if (strlen(str2) != strlen(str1))
        return 0;

    for (; *str1 != '\0'; str1++, str2++)
        if (tolower(*str1) != tolower(*str2))
            return 0;

    return 1;
}