/***************************************************
 *                                                 *
 *       Funzioni di utilità per i timestamp       *
 *                                                 *
 **************************************************/

#include "time.h"
#include <string.h>

/*
 * Converte il timestamp in una stringa formattata (di lunghezza massima 'len') che inserisce in 'str'
 */
void format_timestamp(time_t timestamp, char str[], int len) {
    // Fonte: https://stackoverflow.com/a/9101683
    strftime(str, len, "%d %b %Y %H:%M:%S", localtime(&timestamp));
}