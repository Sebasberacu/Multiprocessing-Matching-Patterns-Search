#define pid_t processes [PROCESS_POOL_SIZE];

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

    int contadorHijos = 0;
    struct message msgParent;
    msgParent.type = (int)processes[contadorHijos];
    msgsnd(msqid, &msgParent, sizeof(msgParent), IPC_NOWAIT); // Manda a leer al primero
    
    do{
        msgrcv(msqid, &msgParent, sizeof(msgParent), 1, IPC_NOWAIT); // Ya leyo porque type=1
        contadorHijos++;
        msgParent.type = (int)processes[contadorHijos];
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