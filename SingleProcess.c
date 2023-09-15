#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <unistd.h>

#define READING_BUFFER 8192
#define PROCESS_POOL_SIZE 3

int main() {
    char ch;

    // Opening file in reading mode
    FILE* ptr = fopen("Texto.txt", "r");
    fseek(ptr, 0, SEEK_SET);  // Posiciona el puntero en la posición de inicio
                              // que le toque

    if (NULL == ptr) {  // Validación si no abre
        printf("file can't be opened \n");
    }

    // Printing what is written in file
    // character by character using loop.
    do {
        ch = fgetc(ptr);
        printf("%c", ch);

        if (ch == '\n') {
            printf("Estoy en la posicion %ld\n\n", ftell(ptr));
        }

        // Checking if character is not EOF.
        // If it is EOF stop reading.
    } while (ch != EOF);

    // Closing the file
    fclose(ptr);

    // manda el mensaje: La posicion, posicion por linea, string("")
    return 0;
}
