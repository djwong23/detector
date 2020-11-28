#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

struct wordNode {
    char *word;
    double numWords;  //needs to be a double
    double dProb;  //needs to be a double
    struct wordNode *next;
};

struct fileNode {
    char *fileName;
    int wordCount;
    struct fileNode *next;
    struct wordNode *words;
};

struct JSDNode {
    char *token;
    double meanProb;
    double firstProb;
    double secondProb;
};

struct finalValNode {
    char *firstName;
    char *secondName;
    double jsd;
    int numTokens;
};

struct arguments {
    char *pathName;
    struct fileNode *head;
    void *lock;
};

//function for tokenizing - file handling
//  check file OK and accessible
//  create wordNode wordHead of LL
//  int totalWords = 0
//  for each character from 0 to lseek (seekend)
//      if(isAlpha) || '-'
//          add to token string
//      if ' ' or '\n'
//          check if token string is already in LL
//              if yes, increment count
//              else add new wordNode
//          totalWords++
//  close file
//  for LL
//      calculate dProb
//  create filNode for file, add LL to its data
//  lock mutex
//      push fileNode to shared DS by number of words
//  unlock mutex
void *handleFile(void *input) {
    struct arguments *args = (struct arguments *) input;
//  printf("This is %s\n", args->pathName);

    return NULL;
}

//function for directories
//  accepts directory path, pointer to shared data structure, mutex
//  check if directory is valid
//  iterate through directory, count number of items
//  malloc pthread array
//  for each item in directory
//      if item is a file
//          pthread create fileHandling function
//      if item is a directory
//          pthread create directories function
//          add pthread to array
//  for each pthread in array
//      pthread_join
//  close directory
//directories doesn't directly use mutex bc it doesn't write to anything
//just passes it to filehandler
void *handleDirectory(void *input) {
    struct arguments *args = (struct arguments *) input;
    char *dirPath = args->pathName;
    printf("Examining %s\n", dirPath);
    DIR *currDir = opendir(dirPath);
    if (currDir == NULL) {
        printf("Directory %s is inaccessible. Returning...\n", dirPath);
        return NULL;
    }
    pthread_t *pThreads = malloc(sizeof(pthread_t) * 10); //need to track all pthreads for joining
    int numPThreads = 10;
    int currPThread = 0;
    while (currDir != NULL) {
        errno = 0;
        struct dirent *entry = readdir(currDir);
        if (entry == NULL && errno == -1) { //file that can't be accessed will not terminate program
            printf("Directory entry is inaccessible. Continuing...\n");
            continue;
        }
        if (entry == NULL) {
            printf("End of directory %s reached.\n", dirPath);
            break;

        }
        if (entry->d_type == DT_DIR && (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)) {
            continue;
        }
        if (entry->d_type == DT_REG || entry->d_type == DT_DIR) {
            pthread_t thread;
            if (currPThread == numPThreads) { //dynamically resizing pthread array
                numPThreads *= 2;
                pThreads = realloc(pThreads, sizeof(pthread_t) * numPThreads);
            }
            struct arguments *newArgs = malloc(sizeof(struct arguments));
            char *newPathName = malloc(strlen(args->pathName) + strlen(entry->d_name) + 2);
            strcpy(newPathName, args->pathName);
            strcat(newPathName, entry->d_name);
            strcat(newPathName, "/\0");
            newArgs->pathName = malloc(strlen(newPathName) + 1);
            strcpy(newArgs->pathName, newPathName);
            newArgs->head = args->head;
            newArgs->lock = args->lock;
            if (entry->d_type == DT_DIR)
                pthread_create(&thread, NULL, handleDirectory, newArgs);
            else
                pthread_create(&thread, NULL, handleFile, newArgs);
            pThreads[currPThread++] = thread;
        }

    }

    printf("Joining threads...\n");
    for (int i = 0; i < currPThread; i++) {
        pthread_join(pThreads[i], NULL);
    }
    return NULL;
}


//analysis
//  do a nested loop to compare two files at a time
//  getMergedTokens - JSD
//  comp

//accepts pointers two file nodes for comparison, pointer to finalValNode LL, pointer to shared data structure

//jensen shannon distance function
//  create two pointers for tokens in the two files we're comparing:
//      if the two pointers point to same token, add their name and meanprobs to the JSDNode linked list and advance their pointers
//      else if they do not point to the same token, add the token that is alphabetically first to the JSDNode linked list, setting one of the discrete probabilities to 0, the other to the discrete probability of the file that contains it, and the mean probability to the mean. Advance only that token
//      continue until both pointers are NULL
//  create running sum for each file
//  for every node in JSDNodes:
//      add file's discrete prob * log (discrete prob/mean prob) to the respective file's running sum
//          when token is not in file this should evaluate to 0 because its discrete prob will be 0
//  compute mean of the running sums to get the JSD of the two files
//  create a finalValNode with the names of the two files, their JSD, and the total number of tokens for the files
//  insert node into linked list of finalValNodes sorted based on tokens

int main(__unused int argc, char **argv) {
    char *directory = malloc(strlen(argv[1]) + 2);
    strcpy(directory, argv[1]);
    strcat(directory, "/\0"); //adding / to end of directory ensures it will be read
    printf("%s\n", directory);
    if (opendir(directory) == NULL) {
        printf("Initial directory is inaccessible. Terminating. \n");
        return EXIT_FAILURE;
    }

    struct fileNode *head = malloc(sizeof(struct fileNode));
    pthread_mutex_t lock;
    struct arguments *args = malloc(sizeof(struct arguments));
    args->pathName = malloc(strlen(directory) + 1);
    strcpy(args->pathName, directory);
    args->head = head;
    args->lock = &lock;
    handleDirectory(args);
    return 0;
}
