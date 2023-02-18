/***************************************************
 *                                                 *
 *       Funzioni di utilità per le stringhe       *
 *                                                 *
 **************************************************/

/*
 * Sostituisce, se presente, il carattere di new-line (\n) con il terminatore di stringa (\0)
 */
void remove_new_line(char* string);

/*
 * Recupera la prima parola dopo uno spazio e la inserisce in 'result'.
 * Conterrà solo il terminatore di stringa (\0) se non c'è nessuno spazio o nessuna parola dopo uno spazio.
 */
void get_first_word_after_space(char* string, char* result);

/*
 * Controlla se due le stringhe sono uguali ignorando le lettere maiuscole e minuscole.
 * Restituisce 1 se sono uguali, 0 se sono diverse.
 */
int equals_ignore_case(char* str1, char* str2);