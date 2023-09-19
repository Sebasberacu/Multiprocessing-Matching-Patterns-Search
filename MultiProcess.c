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

struct message {
    long type;  // Message type
    int filePosition;
    char matchesFound[100];  // Lines that matched regex pattern
} msg;

int createProcesses() {
    for (int i = 0; i < PROCESS_POOL_SIZE; i++) {
        if (fork() != 0)
            processes[i] = (long)i + 1;
        else
            return i + 1;
    }
    return 0;
}

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

int isQueueEmpty(int msqid) {
    struct msqid_ds buf;
    if (msgctl(msqid, IPC_STAT, &buf) == -1) {
        perror("msgctl");
        exit(1);
    }
    return (buf.msg_qnum == 0) ? 1 : 0;
}


int readFile(pid_t processID, int lastPosition, char *fileName) {
    printf("Child process %d STARTS READING.\n", processID);
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

void searchPattern(int processID) {
    printf("Child process %d STARTS SEARCHING.\n", processID);

    int paraHacerAlgo = 0;
    for (int i = 0; i < 10000; i++) {
        paraHacerAlgo += i;
        if (paraHacerAlgo > 100000000) paraHacerAlgo = 0;
    }

    printf("Child process %d ENDED SEARCHING.\n", processID);
}
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
               0);     // recibe tipo = su pid
        msg.type = 1;  // 1 para que solo reciba el padre
        msg.filePosition =
            readFile(childID, msg.filePosition,
                     fileName);  // Guarda la posicion del ultimo salto
        msgsnd(msqid, (void *)&msg, sizeof(msg.matchesFound),
               0);  // Manda mensaje al padre para informar que termino de leer
        searchPattern(childID);  // Procesa los bytesLeidos leidos
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
        // reinicia lectura de hijos si llega al ultimo
        childCounter =
            (childCounter + 1 >= PROCESS_POOL_SIZE) ? 0 : childCounter + 1;
        msg.type = processes[childCounter];  // Pasar el turno al siguiente hijo
        msgsnd(msqid, (void *)&msg, sizeof(msg.matchesFound), 0);
        // Esperar a que algun hijo termine de procesar para imprimir
    } while (msg.filePosition <=
             fileSize);  // mientras no se haya terminado de leer

    msgctl(msqid, IPC_RMID, NULL);  // Eliminar la cola de mensajes

    return 0;
}