#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define numChildren 5
#define READ_PIPE 0
#define WRITE_PIPE 1
#define TIME_TO_RUN 30
#define MAX_SLEEP_TIME 2

#define TIME_BUFF_SIZE 10
#define BUFF_SIZE 64

clock_t startClock;

int **getPipes() {
    int i, **pipes = (int **) malloc(numChildren*sizeof(int *));
    for(i = 0; i < numChildren; i++) {
        pipes[i] = (int *) malloc(2*sizeof(int));
        if(pipe(pipes[i]) == -1) {
            fprintf(stderr, "pipe(2) failed with errno %d\n", errno);
            exit(errno);
        }
    }
    return pipes;
}

// assumes pipes have been closed already
void freePipes(int ***pipes) {
    int i;
    for(i = 0; i < numChildren; i++) {
        free((*pipes)[i]);
    }
    free((*pipes));
}

/* @retval a heap allocated char * of the form "X:YY.ZZZ:" where 
 *      X is the number of minutes
 *      YY is the number of seconds
 *      ZZZ is the number of milliseconds
 */
char *getTime() {
    char *retval = (char *)calloc(TIME_BUFF_SIZE, sizeof(char));
    clock_t currentClock = clock();
    float timeSinceStart = ((float)(currentClock-startClock))/CLOCKS_PER_SEC;
    int secsSinceStart = (int)timeSinceStart;
    float msSinceStart = timeSinceStart-secsSinceStart; 
    snprintf(retval, TIME_BUFF_SIZE, "0:%02d.%.3f:", secsSinceStart, msSinceStart);
    return retval;
}

void readFromPipes(int ***pipesRef) {
    struct timeval tv;
    char buff[BUFF_SIZE], *timeBuff;
    fd_set readSet[numChildren];
    int i, selectVal, readVal, **pipes = *pipesRef, largestFD = 0;
    time_t startTime = 0;
    FILE *outputFile = fopen("output.txt", "w");
    if(outputFile == NULL) {
        fprintf(stderr, "Could not open output.txt with errno %d\n", errno);
        exit(errno);
    }

    tv.tv_sec = 4;
    tv.tv_usec = 0;
    for(i = 0; i < numChildren; i++) {
        close(pipes[i][WRITE_PIPE]);
        if(pipes[i][READ_PIPE] > largestFD) {
            largestFD = pipes[i][READ_PIPE];
        }
    }
    largestFD++;
    
    startTime = time(0);
    startClock = clock();
    while(time(0)-startTime < TIME_TO_RUN) {
        FD_ZERO(&readSet[i]);
        for(i = 0; i < numChildren; i++) {
            FD_SET(pipes[i][READ_PIPE], &readSet[i]);
            selectVal = select(pipes[i][READ_PIPE]+1, &readSet[i], NULL, NULL, &tv);
            printf("%d selectVal\n", selectVal);
            tv.tv_sec = 4;
            tv.tv_usec = 0;
            if(selectVal == -1) {
                fprintf(stderr, "select failed with errno %d\n", errno);
            } else if(selectVal) {
                // read from a pipe
                if((readVal = read(pipes[i][READ_PIPE], &buff, BUFF_SIZE)) > 0) {
                    timeBuff = getTime();
                    fwrite(timeBuff, sizeof(char), TIME_BUFF_SIZE-1, outputFile);
                    fwrite(buff, sizeof(char), readVal-1, outputFile);
                    printf("timeBuff = %s buff=%s", timeBuff, buff);
                    memset(buff, 0, sizeof(char)*BUFF_SIZE);
                    free(timeBuff);
                    timeBuff = NULL;
                } else {
                    fprintf(stderr, "read failed with errno %d\n", errno);
                }
            } 
        }
    }

    fclose(outputFile);
    for(i = 0; i < numChildren; i++) {
        close(pipes[i][READ_PIPE]);
    }
}

/* @param pipe
 *      a reference to an int[2], which represents the read and write pipes
 */
void writeToPipe(int **pipe, int childNum) {
    time_t startTime;
    int sleepTime, messageNum = 0;
    char buff[BUFF_SIZE], *timeBuff;
    close((*pipe)[READ_PIPE]);

    startTime = time(0);
    startClock = clock();
    while(time(0)-startTime < TIME_TO_RUN) {
        messageNum++;
        sleepTime = rand()%(MAX_SLEEP_TIME+1);
        if(sleepTime != 0) {
            sleep(sleepTime);
        }
        timeBuff = getTime();
        snprintf(buff, BUFF_SIZE, "%s Child %d message %d\n", timeBuff, childNum, messageNum);
        write((*pipe)[WRITE_PIPE], buff, sizeof(char)*BUFF_SIZE);
        free(timeBuff);
        timeBuff = NULL;
    }

    close((*pipe)[WRITE_PIPE]);
}

void lastChild(int **pipe) {
    time_t startTime = time(0);
    char *line = NULL;
    size_t alloced = 0;
    ssize_t nread;
    int messageNum = 0;
    char buff[BUFF_SIZE], *timeBuff;

    startTime = time(0);
    startClock = clock();
    while(time(0)-startTime < TIME_TO_RUN) {
        if((nread = getline(&line, &alloced, stdin)) != -1) {
            messageNum++;
            timeBuff = getTime();
            snprintf(buff, BUFF_SIZE, "%s Child 5 message %d\n", timeBuff, messageNum);
            write((*pipe)[WRITE_PIPE], buff, sizeof(char)*BUFF_SIZE);
            free(timeBuff);
            timeBuff = NULL;
        }
        free(line);
        line = NULL;
    }   
}

void makeChildren(int ***pipes) {
    int i, pids[numChildren];
    for(i = 0; i < numChildren; i++) {
        pids[i] = fork();
        if(i == numChildren-1 && pids[i] == 0) {
            printf("child %d made\n", i+1); 
            lastChild(&((*pipes)[i]));
            printf("child %d quit\n", i+1); 
            return;
        } else {
            if(pids[i] == 0) {
                printf("child %d made\n", i+1); 
                writeToPipe(&((*pipes)[i]), i+1);               
                printf("child %d quit\n", i+1); 
                return;
            }
        }
    }
    readFromPipes(pipes);
    for(i = 0; i < numChildren; i++) {
        waitpid(pids[i], NULL, 0);
    }
}

int main(int argc, char *argv[]) {
    int **pipes = getPipes();

    makeChildren(&pipes);
    
    freePipes(&pipes);
}
