#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define READING_BUFFER 8192  // Tamaño del bloque en bytes (8KB)
char buffer[READING_BUFFER];

/** Funcion que procesa el texto leido en el buffer.
 *  Parametros: no tiene.
 *  Retorna: no retorna.
*/
void processBuffer(){
    char *ptr = buffer;
    while (ptr < buffer + READING_BUFFER) {//Desde el inicio del array hasta el final
        char *newline = strchr(ptr, '\n');
        if (newline != NULL) {
            size_t lineLength = newline - ptr;
            printf("Linea: %.*s\n", (int)lineLength, ptr);
            ptr = newline + 1;
        } else {
            printf("Linea: %s\n", ptr);
            break;
        }
    }
    memset(buffer, 0, READING_BUFFER);//Limpia el buffer
}

long readFile(pid_t processID, long lastPosition, char *nombreArchivo) {
    printf("Child process %d STARTS READING.\n", processID);
    FILE *archivo = fopen(nombreArchivo, "rb");
    if (archivo == NULL) {
        perror("Error al abrir el archivo");
        return 1;
    }

    long posicionUltimoSalto = -1;

    // Pone puntero de lectura donde el ultimo proceso dejo de leer
    printf("LastPosition recibida: %ld\n", lastPosition);
    if (fseek(archivo, lastPosition == 0 ? 0 : lastPosition + 1, SEEK_SET) !=
        0) {
        perror("Error al establecer la posición de lectura");
        fclose(archivo);
        return 1;
    }
    // Lee 8K a partir de la posicion que me puso fseek.
    long bytesLeidos = fread(buffer, 1, READING_BUFFER, archivo);
    if (bytesLeidos > 0)  // verifica si se han leído bytes desde el archivo
        // Busca de atras para delante
        for (long i = bytesLeidos - 1; i >= 0; i--) {
            if (buffer[i] == '\n') {
                posicionUltimoSalto = ftell(archivo) - (bytesLeidos - i);
                printf("Ultimo salto de linea detectado: %ld\n",
                       posicionUltimoSalto);
                break;
            } else {
                // Quita caracteres que esten despues del ultimo salto de linea.
                buffer[i] = '\0';
            }
        }
    fclose(archivo);
    printf("\nChild process %d ENDED READING.\n", processID);
    return posicionUltimoSalto;  //-1 si no encontro ultimo salto de linea
}
int main() {
    //Ejemplo de como se procesa el archivo por bloques de 8K.
    pid_t pid = getpid();
    long pos = readFile(pid, 0, "Texto.txt");
    processBuffer();
    long pos2 = readFile(pid, pos, "Texto.txt");
    processBuffer();
    long pos3 = readFile(pid, pos2, "Texto.txt");
    processBuffer();
    return 0;
}
