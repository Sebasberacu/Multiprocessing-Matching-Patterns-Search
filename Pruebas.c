#include <stdio.h>
#include <stdlib.h>

#define BLOCK_SIZE 8192  // Tamaño del bloque en bytes (8KB)

int main() {
    FILE* fp;
    fp = fopen("Texto.txt", "r");

    // Moving pointer to end
    fseek(fp, 8192, SEEK_SET);

    // Leer y mostrar el contenido línea por línea
    char linea[100]; // Puedes ajustar el tamaño del búfer según tus necesidades
    while (fgets(linea, sizeof(linea), fp) != NULL) {
        printf("%s", linea);
    }

    // Cierra el archivo cuando hayas terminado de leerlo
    fclose(fp);

    return 0;
}
