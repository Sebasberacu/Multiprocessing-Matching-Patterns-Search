#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <unistd.h>

#define READING_BUFFER 8192
#define PROCESS_POOL_SIZE 3

struct message {
    int type; // Message type
    char matchesFound[READING_BUFFER];  // Lines that matched regex pattern
};

void readFile(int processID){
    printf("Child process %d READING.\n", processID);
    //sleep(3);
}

void searchPattern(int processID){
    printf("Child process %d SEARCHING.\n", processID);
    //sleep(1);
}

int main() {
    int status;
    key_t msqkey = 999;
    int msqid = msgget(msqkey, IPC_CREAT | 0666); // Crear la cola de mensajes

    for (int i = 0; i < PROCESS_POOL_SIZE; i++) {
        pid_t pid = fork();
        if (pid == 0) { // Proceso hijo
            struct message msg;

            msgrcv(msqid, &msg, sizeof(msg.matchesFound), i+1, IPC_NOWAIT); // mensaje para empezar a leer

            readFile(i+1); // Lee

            msg.type = i+1; // Terminó de leer
            msgsnd(msqid, &msg, sizeof(msg.matchesFound), IPC_NOWAIT); // Avisar que terminó de leer

            searchPattern(i+1);
            exit(0); // Matar proceso hijo
        }
            
    }

    struct message msgParent; // Manejo mensajes proceso padre

    // Make first child run
    msgParent.type = 1;
    msgsnd(msqid, &msgParent, sizeof(msgParent.matchesFound), IPC_NOWAIT);

    for (int i=0; i < PROCESS_POOL_SIZE; i++){ 
        msgrcv(msqid, &msgParent, sizeof(msgParent.matchesFound), i+1, IPC_NOWAIT); // Espero a que termine de leer
        msgParent.type = msgParent.type + 1; // Set va el siguiente leyendo
        msgsnd(msqid, &msgParent, sizeof(msgParent.matchesFound), IPC_NOWAIT); // Manda a leer al siguiente
    }

    // Esperar a todos los procesos
    for (int i = 0; i < PROCESS_POOL_SIZE; i++){ 
        wait(&status);
    }

    // Eliminar la cola de mensajes
    msgctl(msqid, IPC_RMID, NULL);

    return 0;
}



// Driver code
// int main()
// {
// 	char ch;

// 	// Opening file in reading mode
// 	FILE * ptr = fopen("sample.txt", "r");
//     fseek(ptr, (long)10, SEEK_SET); // Posiciona el puntero en la posición de inicio que le toque

// 	if (NULL == ptr) { // Validación si no abre
// 		printf("file can't be opened \n");
// 	}

// 	// Printing what is written in file
// 	// character by character using loop.
// 	do {
// 		ch = fgetc(ptr);
// 		printf("%c", ch);

//         if (ch == '\n'){
//             printf("holaa\n\n");
//         }

// 		// Checking if character is not EOF.
// 		// If it is EOF stop reading.
// 	} while (ch != EOF);

// 	// Closing the file
// 	fclose(ptr);

//  //manda el mensaje: La posicion, posicion por linea, string("")
// 	return 0;
// }

/*
void search() {
    // Abrir el archivo y colocar el puntero de lectura en la posición de inicio
    FILE* file = fopen(file_path, "r");
    fseek(file, start_position, SEEK_SET);

    char line[MAX_LINE_LENGTH];
    SearchResult result;

    // Inicializar las estructuras de resultados
    result.process_id = process_id;
    result.paragraphs = initialize_paragraph_list(); // Crea una lista de párrafos vacía

    while (ftell(file) < end_position) {
        if (fgets(line, sizeof(line), file) == NULL) {
            break; // Fin del archivo
        }

        // Realizar la búsqueda en la línea actual utilizando regex.h
        if (regex_search(line, pattern)) {
            // La línea contiene la expresión regular, se agrega al párrafo actual
            append_line_to_paragraph(&result.paragraphs, line);
        }
    }

    fclose(file);
    
    // Notificar al proceso padre sobre la finalización y los resultados
    notify_parent(result);
}

*/
