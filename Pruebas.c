#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define READING_BUFFER 8192  //8K
char buffer[READING_BUFFER];
int lineCounter = 1;  // Contador de líneas

/** Funcion que procesa el texto leido en el buffer.
 *  Parametros:
 *    - regexStr: La expresión regular a buscar en las líneas.
 *  Retorna: no retorna.
 */
void processBuffer(const char *regexStr) {
    printf("Child process %d STARTS PROCESSING.\n", getpid());
    char *ptr = buffer;
    regex_t regex;
    if (regcomp(&regex, regexStr, REG_EXTENDED) != 0) {
        fprintf(stderr, "Error al compilar la expresión regular.\n");
        exit(1);
    }
    while (ptr < buffer + READING_BUFFER) {
        char *newline = strchr(ptr, '\n');
        if (newline != NULL) {
            size_t lineLength = newline - ptr;
            char line[lineLength + 1];
            strncpy(line, ptr, lineLength);
            line[lineLength] = '\0';
            if (regexec(&regex, line, 0, NULL, 0) == 0)
                printf("Coincidencia en la línea %d: %s\n", lineCounter, line);
            ptr = newline + 1;
            lineCounter++;
        } else {
            char line[READING_BUFFER];
            strcpy(line, ptr);
            if (regexec(&regex, line, 0, NULL, 0) == 0)
                printf("Coincidencia en la línea %d: %s\n", lineCounter, line);

            break;
        }
    }
    regfree(&regex);
    memset(buffer, 0, READING_BUFFER);
    printf("Child process %d ENDED PROCESSING.\n", getpid());
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
    printf("Child process %d ENDED READING.\n", processID);
    return posicionUltimoSalto;  //-1 si no encontro ultimo salto de linea
}
int main() {
    // Ejemplo de como se procesa el archivo por bloques de 8K.
    pid_t pid = getpid();
    long pos = readFile(pid, 0, "Texto.txt");
    processBuffer("graciosa");
    long pos2 = readFile(pid, pos, "Texto.txt");
    processBuffer(
        "graciosa");
    long pos3 = readFile(pid, pos2, "Texto.txt");
    processBuffer("graciosa");
    return 0;
}
