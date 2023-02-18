#include "../costanti.h"

struct record_registro {
    char username[USERNAME_LEN]; // Username dell'utente
    int port; // Porta di ascolto di un device

    /*
     * Socket di un device (grazie a questo posso individuare quale utente mi sta inviando messaggi).
     * Vale INVALID_SOCKET se l'utente è offline.
     */
    int socket;

    time_t login_timestamp; // Timestamp di login
    time_t logout_timestamp; // Timestamp di logout. Vale 0 se l'utente è online.
    struct record_registro* next; // Elemento successivo (è una lista)
};
