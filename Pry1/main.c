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
#include <errno.h>

#define READING_BUFFER 8192
#define PROCESS_POOL_SIZE 3

long processes[PROCESS_POOL_SIZE];

struct message {
    long type;  // Message type
    int filePosition;
    char matchesFound[100];  // Lines that matched regex pattern
} msg;

int createProcesses() {
    for (int i = 0; i < PROCESS_POOL_SIZE; i++) {
        if (fork() != 0) processes[i] = (long)i+1;
        else return i+1;
    }
    return 0;
}

int createMessageQueue(){
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

int isQueueEmpty(int msqid){
    struct msqid_ds buf;
    if (msgctl(msqid, IPC_STAT, &buf) == -1) {
        perror("msgctl");
        exit(1);
    }
    return (buf.msg_qnum == 0) ? 1 : 0;
}

int readFile(int processID) {
    //printf("Child process %d STARTS READING.\n", processID);

    int paraHacerAlgo = 0;
    for (int i=0; i<10000; i++){
        paraHacerAlgo += i;
        if (paraHacerAlgo > 100000000) paraHacerAlgo = 0;
    }
    
    //printf("Child process %d ENDED READING.\n", processID);

    return 5; 
}

void searchPattern(int processID) {
    //printf("Child process %d STARTS SEARCHING.\n", processID);

    int paraHacerAlgo = 0;
    for (int i=0; i<10000; i++){
        paraHacerAlgo += i;
        if (paraHacerAlgo > 100000000) paraHacerAlgo = 0;
    }

    //printf("Child process %d ENDED SEARCHING.\n", processID);
}

int main(int argc, char *argv[]) {
    int msqid = createMessageQueue();
    
    if (isQueueEmpty(msqid) == 1) printf("La cola de mensajes esta vacia.\n");
    else printf("La cola de mensajes tiene mensajes.\n");

    sleep(2);

    int fileSize = 697567;


    int childID = createProcesses(); //cada hijo obtiene un ID unico >0. El padre obtiene 0.

    printf("MY CHILD ID IS: %d\n", childID);

    // Solo los hijos entran
    while (childID != 0) {
        // Espera mensaje del padre para empezar a leer
        msgrcv(msqid, &msg, sizeof(msg.matchesFound), childID, 0);  // recibe tipo = su pid
        //printf("HIJO %d RECIBE MENSAJE. EMPIEZA A LEER\n", childID);

        msg.type = 1;  // 1 para que solo reciba el padre
        msg.filePosition = readFile(childID);  // Guarda la posicion del ultimo salto
        
        msgsnd(msqid, (void *)&msg, sizeof(msg.matchesFound), 0);  // Manda mensaje al padre para informar que termino de leer
        //printf("HIJO %d MANDA MENSAJE. EMPIEZA A PROCESAR\n", childID);
        
        searchPattern(childID);  // Procesa los bytesLeidos leidos
    }

    sleep(2);

    // Solo el padre llega aquí.
    int childCounter = 0;
    //struct message msg;
    
    msg.type = (long)processes[childCounter];
    msg.filePosition = 0;
    msgsnd(msqid, (void *)&msg, sizeof(msg.matchesFound), IPC_NOWAIT);  // Manda a leer al primero, no espera porque es el primero

    do {
        msgrcv(msqid, &msg, sizeof(msg.matchesFound), 1, 0);  // Ya leyo porque type=1
        //printf("type: %ld, filePosition: %d\n", msg.type, msg.filePosition);
        
        printf("CHILDCOUNTER: %d\n", childCounter);

        childCounter = (childCounter + 1 >= PROCESS_POOL_SIZE) ? 0 : childCounter+1;
        
        msg.type = processes[childCounter]; // Pasar el turno al siguiente hijo
        msgsnd(msqid, (void *)&msg, sizeof(msg.matchesFound), 0);

    } while (msg.filePosition <= fileSize);  // mientras no se haya terminado de leer

    msgctl(msqid, IPC_RMID, NULL);  // Eliminar la cola de mensajes

    return 0;
}