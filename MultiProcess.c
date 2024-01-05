/**
 * Grep Proyect. Principios de Sistemas Operativos
 * 
 * @file MultiProcess.c
 * @author Sebastián Bermúdez Acuña & Felipe Obando Arrieta
 * @date 2023-09-24
 */

#include <errno.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#define READING_BUFFER 8192
#define PROCESS_POOL_SIZE 3
char buffer[READING_BUFFER]; // For file reading
long processes[PROCESS_POOL_SIZE]; // Save identifier for each process

//pos 0: Child position in array. pos 1: Child state. pos 2: Pid.
long states[PROCESS_POOL_SIZE][3]; // For child processes.


// Communication between processes
struct message {
    long type;  // Message type
    long filePosition;
    long arrayPosition;
    char matchesFound[8000];  // Lines that matched regex pattern
} msg;

/** In charge of creating processes.
 *  Parameters: None
 *  Returns:
 *          0: If the father.
 *          _: Unique child id
 */
int createProcesses() {
    pid_t pid;
    for (int i = 0; i < PROCESS_POOL_SIZE; i++) {
        pid = fork();
        if (pid != 0) {
            processes[i] = (long)i + 1;
            states[i][0]=i; //Child position in array.
            states[i][1]=0; //States -> 0: Available, 1: Reading, 2: Processing
            states[i][2]=pid; //Child pid
        }
        else 
            return i + 1;
    }
    return 0;
}

/** In charge of creating message queue.
 *  Parameters: None
 *  Returns:
 *          int: queue identifier
 */
int createMessageQueue() {
    key_t msqkey = 999;
    int msqid;

    // Delete queue if existent
    if ((msqid = msgget(msqkey, 0666)) != -1) {
        if (msgctl(msqid, IPC_RMID, NULL) == -1) {
            perror("msgctl");
            exit(1);
        }
        printf("Former existent queue eliminated.\n");
    }
    // Creates new message queue
    msqid = msgget(msqkey, IPC_CREAT | S_IRUSR | S_IWUSR);
    return msqid;
}

/** Checks if queue is empty. Development purposes.
 *  Parametros:
 *             int msqid: message queue id
 *  Retorna:
 *             0: If empty.
 *             1: Not empty.
 */
int isQueueEmpty(int msqid) {
    struct msqid_ds buf;
    if (msgctl(msqid, IPC_STAT, &buf) == -1) {
        perror("msgctl");
        exit(1);
    }
    return (buf.msg_qnum == 0) ? 1 : 0;
}

/** Function that reads the specified file.
 *  Reads in 8K blocks.
 *  Parameters:
 *            pid_t processID: id of the proccess that will read.
 *            int lastPosition: last position read from file.
 *            char * fileName: file name.
 *  Returns:
 *           int: last jump position
 */
int readFile(pid_t processID, long lastPosition, char *fileName) {
    if (lastPosition == -1) {  // Nothing more to read.
        sleep(1);
        exit(1);
    }

    FILE *file = fopen(fileName, "rb"); // Open file
    if (file == NULL) {
        perror("Error while opening file.");
        return 1;
    }

    long lastJumpPosition = -1; //Default -1 if end of file is reached

    // Places reading pointer where the last process ended reading
    if (fseek(file, lastPosition == 0 ? 0 : lastPosition + 1, SEEK_SET) != 0) {
        perror("Error while establishing reading position.");
        fclose(file);
        return 1;
    }
    // Reads 8K from the position given by fseek
    long bytesRead = fread(buffer, 1, READING_BUFFER, file);
    if (bytesRead > 0)
        // From back to front
        for (long i = bytesRead - 1; i >= 0; i--) {
            if (buffer[i] == '\n') {
                lastJumpPosition = ftell(file) - (bytesRead - i);
                break;
            } else {
                // Removes characters after the last jump ('\n')
                buffer[i] = '\0';
            }
        }
    fclose(file);
    return lastJumpPosition;
}

/** Processes the text read in the buffer and checks for regex coincidences.
 *  Parameters:
 *            int msqid: message queue id
 *            pid_t processID: id of the process searching
 *            char* regexStr: Looked regular expression.
 *            long arrayPosition: current position in array of processes
 */
void searchPattern(int msqid ,pid_t processID, const char *regexStr,long arrayPosition) {
    strcpy(msg.matchesFound, "");
    char * ptr = buffer;
    regex_t regex;
    if (regcomp(&regex, regexStr, REG_EXTENDED) != 0) {
        fprintf(stderr, "Error processing regular expression.\n");
        exit(1);
    }

    char coincidences[8000] = "";  // To save coincidences found at buffer
    while (ptr < buffer + READING_BUFFER) {
        char *newline = strchr(ptr, '\n');
        if (newline != NULL) {
            size_t lineLength = newline - ptr;
            char line[lineLength + 1];
            strncpy(line, ptr, lineLength);
            line[lineLength] = '\0';
            if (regexec(&regex, line, 0, NULL, 0) == 0) {
                // Save in array to send it to father later
                strcat(line, "|");  // Delimiter
                strcat(coincidences, line);
            }
            ptr = newline + 1;
        } else {
            char line[READING_BUFFER];
            strcpy(line, ptr);
            if (regexec(&regex, line, 0, NULL, 0) == 0)
                strcat(line,"|");
                strcat(coincidences,line);
            break;
        }
    }
    regfree(&regex);
    memset(buffer, 0, READING_BUFFER); // Free memory
    
    // Send coincidences to father
    msg.type=100;
    msg.arrayPosition=arrayPosition;
    coincidences[sizeof(coincidences) - 1] = '\0';
    // Copies content in coincidences at msg.matchesFound
    strcpy(msg.matchesFound, coincidences);
    msgsnd(msqid, (void *)&msg, sizeof(msg.matchesFound),0);
    strcpy(buffer, ""); // Clean buffer
}

/** Check if all childs are available
 *  Parameters: None
 *  Returns: 
 *            0: all children available.
 *            _: not all children are available.
*/
int childrenAvailable(){
    int sum=0;
    for (int i=0;i<PROCESS_POOL_SIZE;i++)
        sum+=states[i][1];
    return sum;
}

// Receives the file name and the regular expression in the program arguments
int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Correct use: %s <argumento1> <argumento2>\n", argv[0]);
        return 1;
    }
    printf("File received: %s\n", argv[1]);
    printf("Regular expression received: %s\n", argv[2]);

    char * fileName = argv[1]; // Gets the file name entered
    char * regex = argv[2]; // Gets the regular expression entered

    // To measure execution time
    struct timeval start, end;
    long seconds, microseconds;
    gettimeofday(&start, NULL);

    int msqid = createMessageQueue(); // Creates message queue
    sleep(1);

    // Create all child processes
    int childID = createProcesses();  // every child obtains an unique ID >0. Father obtains 0.
    
    // Only children loop here
    long childArrayPosition;
    while (childID != 0) {
        // Waits for the father message to start reading
        msgrcv(msqid, &msg, sizeof(msg.matchesFound), childID,0);  // receives, message type = their id
        
        childArrayPosition=msg.arrayPosition; 
        msg.type = childID; 
        // Starts reading
        msg.filePosition = readFile(childID, msg.filePosition,fileName);  // Saves last jump position
        msg.arrayPosition=childArrayPosition;
        
        msgsnd(msqid, (void *)&msg, sizeof(msg.matchesFound),0);  // Sends message to father. Finished reading
        searchPattern(msqid, childID, regex, childArrayPosition);  // Process data. Looks for regex match.
    }
    sleep(2);
    
    // Only father enters here.
    int childCounter = 0;
    int activeChild = (long)processes[childCounter];

    msg.type = (long)processes[childCounter];
    msg.arrayPosition = states[0][0]; // To know which children sends the message (array position of states[i][0])
    msg.filePosition = 0;
    msgsnd(msqid, (void *)&msg, sizeof(msg.matchesFound),IPC_NOWAIT);  // Tells first child to start reading.
    states[0][1]=1; // Sets first child state in 1: Reading.
    const char delimiter[] = "|"; // To separate coincidences sent by child
    char * token ; // To print the coincidences of the regular expression.
    long communicatedChildPosition = 0; // Position in "states" array
    int coincidencesFound = 1;

    while(1){
        msgrcv(msqid, &msg, sizeof(msg.matchesFound), activeChild,0);  // Waits for a child to end reading
        communicatedChildPosition = msg.arrayPosition; // Saves the child that just read
        
        if (msg.filePosition == -1)  // Nothing else to read. End of file.
            break;

        // Change the state of the child that just read 
        states[communicatedChildPosition][1]=2; // Sets child state 2: Processing
        
        childCounter = (childCounter + 1 >= PROCESS_POOL_SIZE) ? 0 : childCounter + 1; // Resets child processes pool.
        activeChild = processes[childCounter];
        msg.type = processes[childCounter]; // Set next children.
        msg.arrayPosition =  childCounter;
        msgsnd(msqid, (void *)&msg, sizeof(msg.matchesFound), 0); // Sends next process to read
        states[childCounter][1]=1; // Sets the state of the child that just sent to read in 1: Reading.
        
        // Print coincidences from children that finished. 
        msgrcv(msqid, &msg, sizeof(msg.matchesFound), 100,0); 
        communicatedChildPosition=msg.arrayPosition;
        states[communicatedChildPosition][1]=0; // Sets child state in 0: Available.
        token = strtok(msg.matchesFound, delimiter);
        while (token != NULL) {
            printf("Coincidence #%d: %s\n", coincidencesFound, token);
            token = strtok(NULL, delimiter);
            coincidencesFound++;
        }
    }

    // Wait for all children to end finding coincidences.
    while(1){
        sleep(1);
        if (msgrcv(msqid, &msg, sizeof(msg.matchesFound), 100,IPC_NOWAIT)!=-1){ // If receives a message, prints.
            states[msg.arrayPosition][1]=0; // Children in state 0: Available.
            token = strtok(msg.matchesFound, delimiter);
            while (token != NULL) {
                printf("Coincidence #%d: %s\n", coincidencesFound,token);
                token = strtok(NULL, delimiter);
                coincidencesFound++;
            }
        }
        if (childrenAvailable()==0) // All children available means they all ended processing.
            break;
    }

    // Delete all children.
    for (int i = 0; i < PROCESS_POOL_SIZE; i++) {
        kill(states[i][2], SIGTERM);
    }
    msgctl(msqid, IPC_RMID, NULL);  // Delete message queue.


    gettimeofday(&end, NULL);
    seconds = end.tv_sec - start.tv_sec;
    microseconds = end.tv_usec - start.tv_usec;
    if (microseconds < 0) {
        seconds--;
        microseconds += 1000000;
    }
    printf("Execution time: %ld seconds and %ld microseconds.\n", seconds, microseconds);

    return 0;
}
