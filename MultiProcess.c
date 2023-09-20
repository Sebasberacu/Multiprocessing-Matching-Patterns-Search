#include <errno.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define READING_BUFFER 8192
#define PROCESS_POOL_SIZE 3
char buffer[READING_BUFFER];

long processes[PROCESS_POOL_SIZE];
pid_t pids[PROCESS_POOL_SIZE];

struct message {
    long type;  // Message type
    int filePosition;
    char matchesFound[100];  // Lines that matched regex pattern
    int finish;
} msg;
/** Funcion encargada de crear el pool de procesos.
 *  Parametros: no tiene
 *  Retorna:
 *          0: Si es el padre.
 *          _: Si es un hijo.
 *
 */
int createProcesses() {
    pid_t pid;
    for (int i = 0; i < PROCESS_POOL_SIZE; i++) {
        pid = fork();
        if (pid != 0) {
            processes[i] = (long)i + 1;
            pids[i]=pid;
        }
        else 
            return i + 1;
    }
    return 0;
}
/** Funcion encargada de crear la cola de mensajes.
 *  Parametros: no tiene
 *  Retorna:
 *          int (id de la cola)
 *
 */
int createMessageQueue() {
    key_t msqkey = 999;
    int msqid;

    // Elimina la cola de mensajes si existe
    if ((msqid = msgget(msqkey, 0666)) != -1) {
        if (msgctl(msqid, IPC_RMID, NULL) == -1) {
            perror("msgctl");
            exit(1);
        }
        printf("Cola de mensajes eliminada.\n");
    }

    // Crea una nueva cola de mensajes
    msqid = msgget(msqkey, IPC_CREAT | S_IRUSR | S_IWUSR);
    return msqid;
}
/** Funcion para verificar si la cola está vacía.
 *  Parametros:
 *             int msqid: id de la cola de mensajes.
 *  Retorna:
 *             0: Si está vacía.
 *             1: Si no está vacía.
 *
 */
int isQueueEmpty(int msqid) {
    struct msqid_ds buf;
    if (msgctl(msqid, IPC_STAT, &buf) == -1) {
        perror("msgctl");
        exit(1);
    }
    return (buf.msg_qnum == 0) ? 1 : 0;
}

/** Funcion encargada de leer el archivo.
 *  Lee en bloques de 8K.
 * Parametros:
 *            pid_t processID: Id del proceso que va a leer.
 *            int lastPosition: Ultima posicion que se leyó en el archivo.
 *            char * fileName: Nombre del archivo a leer.
 *  Retorna:
 *           int (ultima posicion leida)
 *
 */
int readFile(pid_t processID, int lastPosition, char *fileName) {
    printf("Child process %d STARTS READING.\n", processID);

    if (lastPosition == -1) {  // Ya no hay nada mas que leer.
        // Decirle al padre que ya no mande a leer a mas hijos.
        printf("Child process %d DOES NOT HAVE ANYTHING TO  READ.\n",
               processID);
        sleep(1);
        return -1;
    }

    FILE *file = fopen(fileName, "rb");
    if (file == NULL) {
        perror("Error al abrir el archivo");
        return 1;
    }

    int posicionUltimoSalto = -1;

    // Pone puntero de lectura donde el ultimo proceso dejo de leer
    printf("LastPosition recibida: %d\n", lastPosition);
    if (fseek(file, lastPosition == 0 ? 0 : lastPosition + 1, SEEK_SET) != 0) {
        perror("Error al establecer la posición de lectura");
        fclose(file);
        return 1;
    }
    // Lee 8K a partir de la posicion que me puso fseek.
    long bytesLeidos = fread(buffer, 1, READING_BUFFER, file);
    if (bytesLeidos > 0)  // verifica si se han leído bytes desde el archivo
        // Busca de atras para delante
        for (long i = bytesLeidos - 1; i >= 0; i--) {
            if (buffer[i] == '\n') {
                posicionUltimoSalto = ftell(file) - (bytesLeidos - i);
                printf("Ultimo salto de linea detectado: %d\n",
                       posicionUltimoSalto);
                break;
            } else {
                // Quita caracteres que esten despues del ultimo salto de linea.
                buffer[i] = '\0';
            }
        }
    fclose(file);
    printf("Child process %d ENDED READING.\n", processID);
    return posicionUltimoSalto;  //-1 si no encontro ultimo salto de linea
}
/** Funcion que procesa el texto leido en el buffer.
 *  Parametros:
 *    - regexStr: La expresión regular a buscar en las líneas.
 *  Retorna: no retorna.
 */
void searchPattern(pid_t processID, const char *regexStr) {
    printf("Child process %d STARTS PROCESSING.\n", processID);
    char *ptr = buffer;
    regex_t regex;
    if (regcomp(&regex, regexStr, REG_EXTENDED) != 0) {
        fprintf(stderr, "Error al compilar la expresión regular.\n");
        exit(1);
    }
    char coincidencias[8000] =
        "";  // Guardar coincidencias encontradas en el buffer.
    while (ptr < buffer + READING_BUFFER) {
        char *newline = strchr(ptr, '\n');
        if (newline != NULL) {
            size_t lineLength = newline - ptr;
            char line[lineLength + 1];
            strncpy(line, ptr, lineLength);
            line[lineLength] = '\0';
            if (regexec(&regex, line, 0, NULL, 0) == 0) {
                // printf("Coincidencia en la línea: %s\n", line);
                //  Guardar en array para enviarselo al padre despues de
                //  procesar todo el buffer
                strcat(line, "|");  // Delimitador
                strcat(coincidencias, line);
            }

            ptr = newline + 1;
        } else {
            char line[READING_BUFFER];
            strcpy(line, ptr);
            if (regexec(&regex, line, 0, NULL, 0) == 0)
                printf("Coincidencia en la línea: %s\n", line);

            break;
        }
    }
    regfree(&regex);
    memset(buffer, 0, READING_BUFFER);
    // Enviar coincidencias al padre.

    //! Este codigo es el que debe ejecutar el padre
    const char delimitador[] = "|";

    char *token = strtok(coincidencias, delimitador);

    while (token != NULL) {
        printf("Coincidencia: %s\n", token);
        token = strtok(NULL, delimitador);
    }
    //! Hasta aqui

    // msgsnd(msqid, (void *)&msg, sizeof(msg.matchesFound),0);
    printf("Child process %d ENDED PROCESSING.\n", processID);
}
/** Funcion encargada de obtener el tamaño del archivo.
 * Parametros:
 *            char * fileName: Nombre del archivo a leer.
 * Retorna:
 *           int (tamaño del archivo)
 */
int getFileSize(char *fileName) {
    FILE *file;
    file = fopen(fileName, "r");
    if (file == NULL) {
        perror("Error opening file");
        return (-1);
    }
    fseek(file, 0, SEEK_END);    // Pone puntero al final
    int fileSize = ftell(file);  // Revisa el len del archivo
    fseek(file, 0, SEEK_SET);    // Vuelve a poner el puntero al principio
    fclose(file);
    return fileSize;
}

int main(int argc, char *argv[]) {
    char *fileName = "Texto.txt";
    char *regex = "graciosa";
    int msqid = createMessageQueue();

    if (isQueueEmpty(msqid) == 1)
        printf("La cola de mensajes esta vacia.\n");
    else
        printf("La cola de mensajes tiene mensajes.\n");

    sleep(2);
    int childID = createProcesses();  // cada hijo obtiene un ID unico >0. El
                                      // padre obtiene 0.
    // Solo los hijos entran
    while (childID != 0) {
        // Espera mensaje del padre para empezar a leer
        msgrcv(msqid, &msg, sizeof(msg.matchesFound), childID,
               0);  // recibe tipo = su pid

        if (msg.finish == 1)  // Ya no hay nada mas que leer.
            exit(childID);

        msg.type = 1;  // 1 para que solo reciba el padre
        msg.filePosition =
            readFile(childID, msg.filePosition,
                     fileName);  // Guarda la posicion del ultimo salto
        msgsnd(msqid, (void *)&msg, sizeof(msg.matchesFound),
               0);  // Manda mensaje al padre para informar que termino de leer
        searchPattern(childID, regex);  // Procesa los bytesLeidos leidos
    }

    sleep(2);

    // Solo el padre llega aquí.

    int fileSize = getFileSize(fileName);
    int childCounter = 0;

    msg.type = (long)processes[childCounter];
    msg.filePosition = 0;
    msgsnd(
        msqid, (void *)&msg, sizeof(msg.matchesFound),
        IPC_NOWAIT);  // Manda a leer al primero, no espera porque es el primero

    do {
        msgrcv(msqid, &msg, sizeof(msg.matchesFound), 1,
               0);  // Ya leyo porque type=1
        // Revisar si ya se leyo todo el archivo
        printf("File position: %d\n", msg.filePosition);
        if (msg.filePosition == -1)  // No hay nada mas que leer.
            break;
        // reinicia lectura de hijos si llega al ultimo
        childCounter =
            (childCounter + 1 >= PROCESS_POOL_SIZE) ? 0 : childCounter + 1;
        msg.type = processes[childCounter];  // Pasar el turno al siguiente hijo
        msgsnd(msqid, (void *)&msg, sizeof(msg.matchesFound), 0);
        // Esperar a que algun hijo termine de procesar para imprimir
    } while (msg.filePosition <=
             fileSize);  // mientras no se haya terminado de leer
    printf("\nPADRE TERMINA\n");
    int status;
    for (int i = 0; i < PROCESS_POOL_SIZE; i++) {
        msg.finish = 1;
        msg.type = processes[i];
        msgsnd(msqid, (void *)&msg, sizeof(msg.matchesFound), 0);
        sleep(1);
        kill(pids[i],SIGKILL);
    }
    msgctl(msqid, IPC_RMID, NULL);  // Eliminar la cola de mensajes
    return 0;
}