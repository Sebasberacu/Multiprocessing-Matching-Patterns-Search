//Proyecto 1 de sistemas operativos 
//Creado por Mariangeles Carranza y Anthony Jiménez-2023 
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>
#include <time.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


#define BUFFER_SIZE 8192 //El tamaño del buffer
#define numProcesos 1 //El número de los procesos

struct message {
  pid_t id;
  long type;
  int flag;
  char *lastNewLine;
} msg;

int main (int argc, char *argv[]) {

    //if (argc != 3) {
    //    printf("El formato correcto para utilizar el programa es: grep ¨'pattern1|pattern2|pattern3|...' archivo¨ \n");
    //    return 1;
   // }

    printf("AHahah");
    clock_t start_time, end_time;
    double elapsed_time;
    int i;
    int contador = 0;
    pid_t pid; //Variable para guardar el id de los procesos
    char *filename = "sample.txt"; //Se guarda el nombre del archivo
    long fileSize; //Variable para guardar el tamaño del archivo
    
    int lista[numProcesos]; //Se crea la cola 
    key_t msqkey = 999; //Key de los mensajes
    int msqid = msgget(msqkey, IPC_CREAT | S_IRUSR | S_IWUSR); //Id para pasar los mensajes

    FILE *fp = fopen(filename, "rb"); //Se abre el archivo
    if (fp == NULL) {
        printf("Error opening file\n");
        return 1;
    }

    fseek(fp, 0L, SEEK_END);
    fileSize = ftell(fp); //Se extrae el tamaño del archivo
    fseek(fp, 0L, SEEK_SET);

    for(i = 0; i < numProcesos; i++) {
      pid = fork();
      lista[i] = pid;
      if (pid == 0)
        break; //Si es hijo se sale
    }
    

    if (pid != 0) {
        msg.type = 1;
        msg.lastNewLine = 0; //Se inicializa en 0 para indicar que es la primera vez
        int pos = 0;
        msg.id = lista[contador];
        msgsnd(msqid, (void *)&msg, sizeof(msg), IPC_NOWAIT);
        while (pos < fileSize) {
            if (contador == numProcesos)
                contador = 0;
            msgrcv(msqid, &msg, sizeof(msg), 2, 0);
            msg.flag = 1;
            if (msg.flag == 1) {
                contador++;
                msg.id = lista[contador];
                pos = pos + BUFFER_SIZE;
                msgsnd(msqid, (void *)&msg, sizeof(msg), IPC_NOWAIT);
            } else if (msg.flag == 2) {
                printf("Se encontraron las coincidencias");
            }
        }
    } else {
        while (1) {
            msgrcv(msqid, &msg, sizeof(msg), 1, 0);
            if (msg.id == getpid()) {
                
                //Aqui lee
                //Manda el mensaje
                //Luego procesa y termina
                msg.flag = 1;
                msg.type = 2;
                msgsnd(msqid, (void *)&msg, sizeof(msg), IPC_NOWAIT);
            }  else if (msg.flag == 3) {
                break;
            }
        } 
        exit(0);
    }
    
    
    return 0;
}