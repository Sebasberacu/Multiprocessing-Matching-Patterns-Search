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
char buffer[READING_BUFFER];//Lectura del archivo
long processes[PROCESS_POOL_SIZE];//Guarda identificador de proceso
long states[PROCESS_POOL_SIZE][3];//0: Pos hijo en array
                                 // 1: Estado hijo
                                 // 2: Pid hijo
struct message {
    long type;  // Message type
    long filePosition;
    long arrayPosition;
    char matchesFound[8000];  // Lines that matched regex pattern
} msg;
/** Funcion encargada de crear el pool de procesos.
 *  Parametros: no tiene
 *  Retorna:
 *          0: Si es el padre.
 *          _: Si es un hijo.
 */
int createProcesses() {
    pid_t pid;
    for (int i = 0; i < PROCESS_POOL_SIZE; i++) {
        pid = fork();
        if (pid != 0) {
            processes[i] = (long)i + 1;
            states[i][0]=i;//Pos del hijo en el array.
            states[i][1]=0;//Estados -> 0: Disponible, 1: Leyendo, 2: Procesando
            states[i][2]=pid;//Pid hijo
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
int readFile(pid_t processID, long lastPosition, char *fileName) {
    if (lastPosition == -1) {  // Ya no hay nada mas que leer.
        printf("Child process %d DOES NOT HAVE ANYTHING TO  READ.\n",processID);
        sleep(1);//Para que no reviente la consola a puro print
        exit(1);
    }

    FILE *file = fopen(fileName, "rb");
    if (file == NULL) {
        perror("Error al abrir el archivo");
        return 1;
    }

    long posicionUltimoSalto = -1;//Retorna -1 si llegó al final del archivo

    // Pone puntero de lectura donde el ultimo proceso dejo de leer
    //printf("LastPosition recibida: %ld en hijo %d\n", lastPosition,processID);
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
                //printf("Ultimo salto de linea detectado: %d\n",posicionUltimoSalto);
                break;
            } else {
                // Quita caracteres que esten despues del ultimo salto de linea.
                buffer[i] = '\0';
            }
        }
    fclose(file);
    return posicionUltimoSalto;  //-1 si no encontro ultimo salto de linea
}
/** Funcion que procesa el texto leido en el buffer.
 *  Parametros:
 *    - regexStr: La expresión regular a buscar en las líneas.
 *  Retorna: no retorna.
 */
void searchPattern(int msqid ,pid_t processID, const char *regexStr,long arrayPosition) {
    strcpy(msg.matchesFound, "");
    char * ptr = buffer;
    regex_t regex;
    if (regcomp(&regex, regexStr, REG_EXTENDED) != 0) {
        fprintf(stderr, "Error al compilar la expresión regular.\n");
        exit(1);
    }
    char coincidencias[8000] = "";  // Guardar coincidencias encontradas en el buffer
    while (ptr < buffer + READING_BUFFER) {
        char *newline = strchr(ptr, '\n');
        if (newline != NULL) {
            size_t lineLength = newline - ptr;
            char line[lineLength + 1];
            strncpy(line, ptr, lineLength);
            line[lineLength] = '\0';
            if (regexec(&regex, line, 0, NULL, 0) == 0) {
                //  Guardar en array para enviarselo al padre despues de procesar todo el buffer
                strcat(line, "|");  // Delimitador
                strcat(coincidencias, line);
            }
            ptr = newline + 1;
        } else {
            char line[READING_BUFFER];
            strcpy(line, ptr);
            if (regexec(&regex, line, 0, NULL, 0) == 0)
                strcat(line,"|");
                strcat(coincidencias,line);
            break;
        }
    }
    regfree(&regex);
    memset(buffer, 0, READING_BUFFER);
    // Enviar coincidencias al padre.
    msg.type=100;//mandar a imprimir
    msg.arrayPosition=arrayPosition;
    coincidencias[sizeof(coincidencias) - 1] = '\0';
    // Copia el contenido de coincidencias en msg.matchesFound
    strcpy(msg.matchesFound, coincidencias);
    msgsnd(msqid, (void *)&msg, sizeof(msg.matchesFound),0);
    strcpy(buffer, "");//Limpia buffer
}
/** Funcion para verificar si todos los hijos estan disponibles.
 *  Parametros: no tiene.
 *  Retorna: 0 si todos los hijos estan disponibles.
 *           _ algun hijo tiene otro estado.
*/
int childrenAvailable(){
    int suma=0;
    for (int i=0;i<PROCESS_POOL_SIZE;i++)
        suma+=states[i][1];
    return suma;
}
//Recibe el nombre del archivo y la expresion regular en los argumentos del programa.
int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: %s <argumento1> <argumento2>\n", argv[0]);
        return 1; // Termina el programa con un código de error
    }
    printf("Nombre de archivo ingresado: %s\n", argv[1]);
    printf("Expresion regular ingresada: %s\n", argv[2]);

    char * fileName = argv[1];//Saca el nombre del archivo que recibe en el argumento
    char * regex = argv[2];//Saca la expresion regular que recibe en el argumento

    int msqid = createMessageQueue();

    if (isQueueEmpty(msqid) == 1)
        printf("La cola de mensajes esta vacia.\n");
    else
        printf("La cola de mensajes tiene mensajes.\n");

    sleep(2);
    int childID = createProcesses();  // cada hijo obtiene un ID unico >0. El padre obtiene 0.
    // Solo los hijos entran
    long arrayPositionHijo;
    while (childID != 0) {
        // Espera mensaje del padre para empezar a leer
        msgrcv(msqid, &msg, sizeof(msg.matchesFound), childID,0);  // recibe tipo = su pid
        arrayPositionHijo=msg.arrayPosition; 
        msg.type = childID; 
        msg.filePosition = readFile(childID, msg.filePosition,fileName);  // Guarda la posicion del ultimo salto
        msg.arrayPosition=arrayPositionHijo;
        msgsnd(msqid, (void *)&msg, sizeof(msg.matchesFound),0);  // Manda mensaje al padre para informar que termino de leer
        searchPattern(msqid,childID, regex,arrayPositionHijo);  // Procesa los datos leidos
    }
    sleep(2);
    // Solo el padre llega aquí.
    int childCounter = 0;

    msg.type = (long)processes[childCounter];
    int activeChild=(long)processes[childCounter];
    msg.arrayPosition = states[0][0];//Para saber cual hijo me envia el mensaje (array position del states[i][0])
    msg.filePosition = 0;
    msgsnd(msqid, (void *)&msg, sizeof(msg.matchesFound),IPC_NOWAIT);  // Manda a leer al primero, no espera porque es el primero
    states[0][1]=1;//Pone el estado del primer hijo en 1: Leyendo
    const char delimitador[] = "|";//Para separar las coincidencias que el hijo envia
    char * token ;//Para imprimir las coincidencias de la expresion regular
    long posHijoComunicado = 0;//Posicion en el arreglo "states"
    int cantidadCoincidencias = 1;

    while(1){
        msgrcv(msqid, &msg, sizeof(msg.matchesFound), activeChild,0);  // Algun hijo termina de leer.
        posHijoComunicado = msg.arrayPosition;//Guarda el hijo que acaba de leer
        if (msg.filePosition == -1)  // No hay nada mas que leer (fin de archivo).
            break;
        //Cambia estado del hijo que me aviso que termino de leer.    
        states[posHijoComunicado][1]=2;//Pone el estado del hijo en 2: Procesando
        // reinicia lectura de hijos si llega al ultimo
        childCounter =(childCounter + 1 >= PROCESS_POOL_SIZE) ? 0 : childCounter + 1;
        activeChild = processes[childCounter];
        msg.type = processes[childCounter];  // Pasar el turno al siguiente hijo
        msg.arrayPosition =  childCounter;
        msgsnd(msqid, (void *)&msg, sizeof(msg.matchesFound), 0);
        states[childCounter][1]=1;//Pone el estado del hijo (que acaba de poner a leer) en 1: Leyendo
        //Imprimir coincidencias de los hijos que ya procesaron
        msgrcv(msqid, &msg, sizeof(msg.matchesFound), 100,0); 
        posHijoComunicado=msg.arrayPosition;
        states[posHijoComunicado][1]=0;//Hijo en estado Disponible
        //Se debe enviar mensaje al mae para que imprima
        token = strtok(msg.matchesFound, delimitador);
        while (token != NULL) {
            printf("Coincidencia #%d: %s\n", cantidadCoincidencias,token);
            token = strtok(NULL, delimitador);
            cantidadCoincidencias++;
        }
    }
    //Esperar a que los hijos terminen de buscar coincidencias
    while(1){
        sleep(1);
        if (msgrcv(msqid, &msg, sizeof(msg.matchesFound), 100,IPC_NOWAIT)!=-1){//Si recibe mensaje imprime
            states[msg.arrayPosition][1]=0;//Hijo en estado Disponible
            //Se debe enviar mensaje al mae para que imprima
            token = strtok(msg.matchesFound, delimitador);
            while (token != NULL) {
                printf("Coincidencia #%d: %s\n", cantidadCoincidencias,token);
                token = strtok(NULL, delimitador);
                cantidadCoincidencias++;
            }
        }
        if (childrenAvailable()==0)//Hijos disponibles implica que ya terminaron de buscar
            break;
    }
    //Ciclo para eliminar a los hijos.
    for (int i = 0; i < PROCESS_POOL_SIZE; i++) {
        kill(states[i][2], SIGTERM);
    }
    msgctl(msqid, IPC_RMID, NULL);  // Eliminar la cola de mensajes
    return 0;
}