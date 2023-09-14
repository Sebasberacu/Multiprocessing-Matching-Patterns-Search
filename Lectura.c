#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUFFER_SIZE 8192
#define MAX_PROCESSES 5

typedef struct {
    int pipefd[2]; // Tubería para comunicación entre el padre y el hijo
    pid_t pid;     // ID del proceso hijo
} ProcessInfo;

int esLineaEnBlanco(const char *linea) {
    for (size_t i = 0; i < strlen(linea); i++) {
        if (linea[i] != ' ' && linea[i] != '\t' && linea[i] != '\n' &&
            linea[i] != '\r') {
            return 0;
        }
    }
    return 1;
}

char *leerArchivo(const char *nombreArchivo) {
    FILE *archivo;
    archivo = fopen(nombreArchivo, "r");
    if (archivo == NULL) {
        perror("Error al abrir el archivo");
        return NULL;
    }
    char buffer[BUFFER_SIZE];
    char *contenido = NULL;
    size_t tamannoContenido = 0;

    while (1) {
        size_t bytesRead = fread(buffer, 1, BUFFER_SIZE, archivo);
        if (bytesRead == 0) {
            break; // Fin del archvo
        }
        tamannoContenido += bytesRead;
        contenido = realloc(contenido, tamannoContenido);
        memcpy(contenido + tamannoContenido - bytesRead, buffer, bytesRead);
    }
    fclose(archivo);
    return contenido;
}

void buscarRegexEnLinea(const char *linea, const char *expresionRegex, int numeroLinea, int numeroProceso) {
    regex_t regex;
    int reti;

    // Compilar la expresión regular
    reti = regcomp(&regex, expresionRegex, REG_EXTENDED);
    if (reti) {
        fprintf(stderr, "Error al compilar la expresión regular\n");
        exit(1);
    }

    // Comparar con la regex
    reti = regexec(&regex, linea, 0, NULL, 0);
    if (!reti) {
        printf("Proceso %d: Coincidencia en línea %d: %s\n", numeroProceso, numeroLinea, linea);
    } else if (reti == REG_NOMATCH) {
        // No hay coincidencia en esta línea
    } else {
        char error_message[100];
        regerror(reti, &regex, error_message, sizeof(error_message));
        fprintf(stderr, "Error en regexec: %s\n", error_message);
        exit(1);
    }

    // Liberar la estructura de la expresión regular
    regfree(&regex);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <expresion-regex> <nombre-archivo>\n", argv[0]);
        return 1;
    }

    const char *expresionRegex = argv[1];
    const char *nombreArchivo = argv[2];

    char *contenido = leerArchivo(nombreArchivo);

    ProcessInfo processInfo[MAX_PROCESSES];
    int currentProcess = 0;
    int lineCount = 1;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (pipe(processInfo[i].pipefd) == -1) {
            perror("Error al crear la tubería");
            return 1;
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("Error al crear el proceso hijo");
            return 1;
        }

        if (pid == 0) {
            // Proceso hijo
            close(processInfo[i].pipefd[0]); // Cerrar el extremo de lectura de la tubería
            char *linea = strtok(contenido, "\n");
            while (linea != NULL) {
                if (!esLineaEnBlanco(linea) && lineCount % MAX_PROCESSES == currentProcess) {
                    buscarRegexEnLinea(linea, expresionRegex, lineCount, i + 1);
                }
                linea = strtok(NULL, "\n");
                lineCount++;
            }
            close(processInfo[i].pipefd[1]); // Cerrar el extremo de escritura de la tubería
            exit(0);
        } else {
            // Proceso padre
            processInfo[i].pid = pid;
            close(processInfo[i].pipefd[1]); // Cerrar el extremo de escritura de la tubería
        }

        currentProcess = (currentProcess + 1) % MAX_PROCESSES;
    }

    for (int i = 0; i < MAX_PROCESSES; i++) {
        wait(NULL); // Esperar a que los procesos hijos terminen
    }

    free(contenido);
    return 0;
}
