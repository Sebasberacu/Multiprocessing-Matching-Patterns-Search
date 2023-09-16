#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <unistd.h>

#define READING_BUFFER 8192
#define PROCESS_POOL_SIZE 3
int processes[PROCESS_POOL_SIZE];
size_t bytesLeidos;

struct message {
    int type;  // Message type
    long filePosition;
    char matchesFound[READING_BUFFER];  // Lines that matched regex pattern
} msg;

/** Funcion responsable de la creación de los hijos.
 * El proceso principal guarda los PID de los hijos un arreglo global.
 * Params: No tiene.
 * Returns: No retorna.
 *
 */
void createProcesses() {
    for (int i = 0; i < PROCESS_POOL_SIZE; i++) {
        pid_t childProcess = fork();
        if (childProcess != 0) {
            processes[i] = childProcess;
        } else {
            break;
        }
    }
}

/** Funcion responsable de la lectura del archivo.
 *  Cada proceso debe colocar el puntero del archivo en la posicion donde
 * desea comenzar a leer, para esto se debe tomar en cuenta la ultima posicion
 * de lectura del proceso anterior. Despues se lee 8K a partir de ultima
 * posicion +1. Retorna la posicion del ultimo salto de linea leido en el
 * bloque.
 *
 */
long readFile(int processID, long lastPosition) {
    printf("Child process %d STARTS READING.\n", processID);
    FILE *archivo;
    char *nombreArchivo = "DonQuijote.txt";
    archivo = fopen(nombreArchivo, "rb");
    if (archivo == NULL) {
        perror("Error al abrir el archivo");
        return 1;
    }
    char buffer[READING_BUFFER];
    long posicionUltimoSalto =
        -1;  //-1 indica que no ha encontrado salto de linea
    // Establece la posición desde la que deseas comenzar la lectura (en bytes)
    // debe empezar donde el ultimo proceso dejo de leer.

    if (fseek(archivo, lastPosition == 0 ? 0 : lastPosition + 1, SEEK_SET) !=
        0) {
        perror("Error al establecer la posición de lectura");
        fclose(archivo);
        return 1;
    }
    printf("Hijo %d debe empezara a leer en posicion: %ld\n", processID,
            lastPosition == 0 ? 0 : lastPosition + 1);
    // Lee 8K a partir de la posicion que me puso fseek.
    bytesLeidos = fread(buffer, 1, READING_BUFFER, archivo);
    if (bytesLeidos > 0) {  // Leyo algo
        // Busca el último salto de línea en el bloque actual de atras para
        // delante
        for (size_t i = bytesLeidos - 1; i >= 0; i--) {
            // busca ultimo salto de linea del bloque leido
            if (buffer[i] == '\n') {
                posicionUltimoSalto = ftell(archivo) - (bytesLeidos - i);
                break;
            }
        }
    }
    fclose(archivo);
    printf("Child process %d ENDED READING.\n", processID);
    sleep(1);
    return posicionUltimoSalto;  //-1 si no encontro ultimo salto de linea
}
/** Funcion responsable de buscar una expresion regular los bytesLeidos leidos
 * por el proceso. Si encuentra coincidencias debe enviar mensaje de
 * notificacion al padre. No retorna nada.
 *
 *
 */
void searchPattern(int msqid, const char *patron) {
    printf("Child process %d STARTS searchPattern.\n", getpid());
    msg.type = 2;
    sleep(3);
    //msgsnd(msqid, &msg, 1024, IPC_NOWAIT);
    printf("Child process %d ENDED searchPattern.\n", getpid());
}

int main(int argc, char *argv[]) {
    int status;
    key_t msqkey = 999;
    int msqid = msgget(msqkey, IPC_CREAT | 0666);  // Crear la cola de mensajes

    char *expresionRegular = "cuenta";
    char *nombreArchivo = "Texto.txt";
    FILE *file = fopen(nombreArchivo, "r");  // abrir el archivo
    fseek(file, 0, SEEK_SET);                // lo posiciono al principio
    long fileSize = ftell(file);

    pid_t parentPid = getpid();
    createProcesses();
    pid_t childPid = getpid();
    // Solo los hijos entran
    while (childPid != parentPid) {
        // Espera mensaje del padre para empezar a leer
        msgrcv(msqid, &msg, 1024,
               (int)childPid,  // recibe tipo = su pid
               IPC_NOWAIT);
        msg.filePosition = readFile(
            childPid, msg.filePosition);  // Guarda la posicion del ultimo salto
                                          // de linea leido
        msg.type = 1;                     // 1 para que solo reciba el padre
        msgsnd(msqid, &msg, 1024, IPC_NOWAIT);  // Manda mensaje al padre para
                                                // informar que termino de leer
        searchPattern(msqid,
                      expresionRegular);  // Procesa los bytesLeidos leidos
    }

    // Solo el padre llega aquí.
    int childCounter = 0;
    //struct message msg;
    msg.type = (int)processes[childCounter];
    msg.filePosition = 0;
    msgsnd(msqid, &msg, 1024, IPC_NOWAIT);  // Manda a leer al primero

    do {
        msgrcv(msqid, &msg, 1024, 1, IPC_NOWAIT);  // Ya leyo porque type=1
        msg.type = (int)processes[childCounter + 1];
        msgsnd(msqid, &msg, 1024, IPC_NOWAIT);
        if (childCounter + 1 == PROCESS_POOL_SIZE) {  // Reutilizar procesos hijos,se utilizaron todos
            childCounter = 0;
        }

    } while ((long)msg.filePosition <= fileSize);  // mientras no se haya terminado de leer

    fclose(file);                   // Cerrar archivo
    msgctl(msqid, IPC_RMID, NULL);  // Eliminar la cola de mensajes

    return 0;
}