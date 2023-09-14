#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <unistd.h>

#define READING_BUFFER 8192
#define PROCESS_POOL_SIZE 3

pid_t processes [PROCESS_POOL_SIZE];

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
    pid_t pid;
    int type; // Message type
    int filePosition;
    char matchesFound[READING_BUFFER];  // Lines that matched regex pattern
};

int readFile(int processID){
    printf("Child process %d READING.\n", processID);
    //sleep(3);
    return 10;
}

void searchPattern(int processID){
    printf("Child process %d SEARCHING.\n", processID);
    //sleep(1);
}

int main(int argc, char *argv[]){
    int status;
    key_t msqkey = 999;
    int msqid = msgget(msqkey, IPC_CREAT | 0666); // Crear la cola de mensajes

    FILE * file = fopen("sample.txt", "r"); // abrir el archivo
    fseek(file, 0, SEEK_SET); // lo posiciono al principio

    pid_t pidPadre = getpid();
    createProcesses();
    pid_t pidHijo = getpid();

    while(pidHijo != pidPadre){ // Solo los hijos entran
        struct message msg;
        msgrcv(msqid, &msg, 256, 1, IPC_NOWAIT); // mensaje para empezar a leer
        if (msg.pid == pidHijo){
            int ultimaPosicion = readFile(pidHijo);
            msg.filePosition = ultimaPosicion;
            msgsnd(msqid, &msg, 256, IPC_NOWAIT); // Manda a leer al siguiente
            searchPattern(pidHijo);
        }
    } 

    struct message msgParent;
    msgParent.pid = processes[0];
    msgsnd(msqid, &msgParent, 256, IPC_NOWAIT); // Manda a leer al primero

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
