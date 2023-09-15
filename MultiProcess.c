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

struct message {
    int type; // Message type
    int filePosition;
    char matchesFound[READING_BUFFER];  // Lines that matched regex pattern
} msg;

/** Funcion responsable de la creación de los hijos.
 * El proceso principal guarda los PID de los hijos un arreglo global.
 * Params: No tiene.
 * Returns: No retorna.
 * 
*/
void createProcesses(){
    for (int i=0; i < PROCESS_POOL_SIZE; i++){
        pid_t childProcess = fork();
        if (childProcess != 0){
            processes[i] = childProcess;
        } else {
            break;
        }
    }
}

int readFile(int processID){
    printf("Child process %d STARTS READING.\n", processID);
    printf("Child process %d ENDED READING.\n", processID);

    return 10;
}

void searchPattern(int processID){
    printf("Child process %d SEARCHING.\n", processID);
    sleep(2);
    printf("Child process %d FINISHED SEARCHING.\n", processID);
}

int main(int argc, char *argv[]){
    int status;
    key_t msqkey = 999;
    int msqid = msgget(msqkey, IPC_CREAT | 0666); // Crear la cola de mensajes

    FILE * file = fopen("Texto.txt", "r"); // abrir el archivo
    fseek(file, 0, SEEK_SET); // lo posiciono al principio
    long fileSize = ftell(file);

    pid_t parentPid = getpid();
    createProcesses();
    pid_t childPid = getpid();

    while(childPid != parentPid){ // Solo los hijos entran
        // mensaje para empezar a leer
        msgrcv(msqid, &msg, 1024,
              (int)childPid, // recibe tipo = su pid
              IPC_NOWAIT);
        msg.filePosition = readFile(childPid);
        msg.type = 1; // 1 para que solo reciba el padre
        msgsnd(msqid, &msg, 1024, IPC_NOWAIT); // Manda a leer al siguiente
        searchPattern(childPid);
    } 
    
    //Solo el padre llega aquí.
    int childCounter = 0;
    //struct message msg;
    msg.type = (int)processes[childCounter];
    msgsnd(msqid, &msg, 1024, IPC_NOWAIT); // Manda a leer al primero

    do{
        msgrcv(msqid, &msg, 1024,
              1,
              IPC_NOWAIT); // Ya leyo porque type=1
        msg.type = (int)processes[childCounter+1];
        msgsnd(msqid, &msg, 1024, IPC_NOWAIT);
        if (childCounter + 1 == PROCESS_POOL_SIZE){ // Reutilizar procesos hijos, si ya se utilizaron todos
            childCounter = 0;
        }
    }while((long)msg.filePosition <= fileSize); // mientras no se haya terminado de leer
    

    fclose(file); // Cerrar archivo
    msgctl(msqid, IPC_RMID, NULL);  // Eliminar la cola de mensajes

    return 0;
}