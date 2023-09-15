#include <stdio.h>
#include <stdlib.h>

#define BLOCK_SIZE 8192 // Tamaño del bloque en bytes (8KB)

int main() {
    FILE *archivo;
    char *nombreArchivo = "DonQuijote.txt"; // Reemplaza con el nombre de tu archivo

    archivo = fopen(nombreArchivo, "rb");
    if (archivo == NULL) {
        perror("Error al abrir el archivo");
        return 1;
    }

    char buffer[BLOCK_SIZE];
    size_t bytesLeidos;
    long posicionUltimoSalto = -1; // Inicializamos con -1, que indica que no se ha encontrado un salto de línea

    while ((bytesLeidos = fread(buffer, 1, BLOCK_SIZE, archivo)) > 0) {
        // Busca el último salto de línea en el bloque actual
        for (size_t i = bytesLeidos - 1; i >= 0; i--) {
            if (buffer[i] == '\n') {
                // Calcula la posición en el archivo sumando la posición en el bloque y la posición actual en el archivo
                posicionUltimoSalto = ftell(archivo) - (bytesLeidos - i);
                break;
            }
        }
        // Si se encontró el último salto de línea en este bloque, guarda su posición en el archivo
        if (posicionUltimoSalto != -1) {
            printf("Posición del último salto de línea en el archivo: %ld\n", posicionUltimoSalto);
        }
    }
    fclose(archivo);
    return 0;
}
