/***************************************************
 *                                                 *
 *       Funzioni di utilità per i timestamp       *
 *                                                 *
 **************************************************/

#include <time.h>

/*
 * Converte il timestamp in una stringa formattata (di lunghezza massima 'len') che inserisce in 'str'
 */
void format_timestamp(time_t timestamp, char str[], int len);