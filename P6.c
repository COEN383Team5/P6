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
#define TIME_TO_RUN 3
#define MAX_SLEEP_TIME 2

#define TIME_BUFF_SIZE 10
#define BUFF_SIZE 32

struct timeval startTV;

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
    struct timeval now;
    char *retval = (char *)calloc(TIME_BUFF_SIZE+1, sizeof(char));
    gettimeofday(&now, NULL);
    int msSinceStart = (now.tv_usec-startTV.tv_usec)/1000;
    int secsSinceStart = now.tv_sec-startTV.tv_sec;
    snprintf(retval, TIME_BUFF_SIZE, "0:%02d.%03d:", secsSinceStart, msSinceStart);
    return retval;
}

void readFromPipes(int ***pipesRef) {
    struct timeval tv;
    char buff[BUFF_SIZE], *timeBuff;
    fd_set readSet;
    int i, readVal, selectVal, **pipes = *pipesRef, largestFD = 0;
    time_t startTime = 0;
    FILE *outputFile = fopen("output.txt", "w");
    if(outputFile == NULL) {
        fprintf(stderr, "Could not open output.txt with errno %d\n", errno);
        exit(errno);
    }

    tv.tv_sec = 0;
    tv.tv_usec = 500000;
    for(i = 0; i < numChildren; i++) {
        close(pipes[i][WRITE_PIPE]);
        if(pipes[i][READ_PIPE] > largestFD) {
            largestFD = pipes[i][READ_PIPE];
        }
    }
    
    startTime = time(0);
    gettimeofday(&startTV, NULL);
    while(time(0)-startTime < TIME_TO_RUN) {
        FD_ZERO(&readSet);
        for(i = 0; i < numChildren; i++) {
            FD_SET(pipes[i][READ_PIPE], &readSet);
        }
        selectVal = select(largestFD+1, &readSet, NULL, NULL, &tv);
        printf("%d selectVal\n", selectVal);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        if(selectVal == -1) {
            fprintf(stderr, "select failed with errno %d\n", errno);
        } else if(selectVal) {
            // read from a pipes that were set by select
            for(i = 0; i < numChildren; i++) {
                if(FD_ISSET(pipes[i][READ_PIPE], &readSet)) {
                    if((readVal = read(pipes[selectVal][READ_PIPE], &buff, BUFF_SIZE)) > 0) {
                        timeBuff = getTime();
                        // subtract 1 to avoid writting NULL byte
                        fwrite(timeBuff, sizeof(char), TIME_BUFF_SIZE-1, outputFile);
                        fwrite(buff, sizeof(char), readVal-1, outputFile);
                        printf("timeBuff = %s buff=%s", timeBuff, buff);
                        memset(buff, 0, sizeof(char)*BUFF_SIZE);
                        free(timeBuff);
                        timeBuff = NULL;
                    } else if(readVal < 0) {
                        fprintf(stderr, "read failed with errno %d\n", errno);
                    }
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
    gettimeofday(&startTV, NULL);
    while(time(0)-startTime < TIME_TO_RUN) {
        messageNum++;
        sleepTime = rand()%(MAX_SLEEP_TIME+1);
        if(sleepTime != 0) {
            sleep(sleepTime);
        }
        timeBuff = getTime();
        snprintf(buff, BUFF_SIZE, "%s Child %d message %d\n", timeBuff, childNum, messageNum);
        write((*pipe)[WRITE_PIPE], buff, strlen(buff)+1);
        printf("wrote %s\n", buff);
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
    gettimeofday(&startTV, NULL);
    while(time(0)-startTime < TIME_TO_RUN) {
        if((nread = getline(&line, &alloced, stdin)) != -1) {
  //          printf("**** CHILD FIVE ****\n");
            messageNum++;
            timeBuff = getTime();
            snprintf(buff, BUFF_SIZE, "%s Child 5 message %d\n\0", timeBuff, messageNum);
            write((*pipe)[WRITE_PIPE], buff, strlen(buff+1));
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
            lastChild(&((*pipes)[i]));
            return;
        } else {
            if(pids[i] == 0) {
                writeToPipe(&((*pipes)[i]), i+1);               
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
