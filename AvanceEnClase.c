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

struct message {
    int type; // Message type
    int filePosition;
    char matchesFound[READING_BUFFER];  // Lines that matched regex pattern
} msg;

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

    FILE * file = fopen("sample.txt", "r"); // abrir el archivo
    fseek(file, 0, SEEK_SET); // lo posiciono al principio
    long fileSize = ftell(file);

    pid_t pidPadre = getpid();
    createProcesses();
    pid_t pidHijo = getpid();

    while(pidHijo != pidPadre){ // Solo los hijos entran
        msgrcv(msqid, &msg, 8000, (int)pidHijo, IPC_NOWAIT); // mensaje para empezar a leer
        
        msg.filePosition = readFile(pidHijo);
        msg.type = 1; // 1 para que solo reciba el padre
        msgsnd(msqid, &msg, 8000, IPC_NOWAIT); // Manda a leer al siguiente
        searchPattern(pidHijo);
    } 
    //Solo el padre llega aquí.
    int contadorHijos = 0;
    struct message msgParent;
    msgParent.type = (int)processes[contadorHijos];
    msgsnd(msqid, &msgParent, sizeof(msgParent), IPC_NOWAIT); // Manda a leer al primero
    
    do{
        msgrcv(msqid, &msgParent, sizeof(msgParent), 1, IPC_NOWAIT); // Ya leyo porque type=1
        msgParent.type = (int)processes[contadorHijos+1];
        msgsnd(msqid, &msgParent, sizeof(msgParent), IPC_NOWAIT);
        if (contadorHijos + 1 == PROCESS_POOL_SIZE){
            contadorHijos = 0;
        }
    }while(1); 
    
    // Eliminar la cola de mensajes
    msgctl(msqid, IPC_RMID, NULL);

    return 0;
    
    

    // while (no se termine de leer){
    //     struct mensajerecibidoPadre
    //     esperando a que le manden un mensaje (mensajerecibidoPadre)
    //     contadorHijosUsados++;
    //     envia un mensaje el siguiente en procesos (con contadorHijosUsados)
    //     if (contadorHijos+1 == POOL_SIZE){
    //         contadorHijosUsados = 0;
    //     }
    // }
}