#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct wordNode {
    char *word;
    double numWords;//needs to be a double
    double dProb;   //needs to be a double
    struct wordNode *next;
};

struct fileNode {
    char *fileName;
    double wordCount;
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
    printf("%s has been accessed\n", args->pathName);
    int fileDesc = open(args->pathName, O_RDONLY);
    if (fileDesc == -1) {
        printf("File %s is inaccessible. Returning...\n", args->pathName);
        return NULL;
    }

    pthread_mutex_lock(args->lock);
    struct fileNode *file = malloc(sizeof(struct fileNode));
    struct fileNode *head = args->head;
    if (head->wordCount == -1) {
        file = head;
    } else {
        struct fileNode *ptr = head;
        while (ptr->next != NULL) {
            ptr = ptr->next;
        }
        ptr->next = file;
    }
    file->fileName = malloc((strlen(args->pathName)) + 1);
    strcpy(file->fileName, args->pathName);
    file->next = NULL;
    file->wordCount = 0;
    file->words = NULL;
    pthread_mutex_unlock(args->lock);

    int fileSize = lseek(fileDesc, 0, SEEK_END);
    lseek(fileDesc, 0, SEEK_SET);
    int maxBufSize = 20;
    char *buf = malloc(maxBufSize);
    //strcpy(buf, "\0");
    int curBufPosition = 0;
    char c = '\0';


    for (int i = 0; i < (int) (sizeof(char) * fileSize); i++) {
        read(fileDesc, &c, sizeof(char));
        if (isalpha(c) || c == '-') {
            c = tolower(c);
            memcpy(&buf[curBufPosition], &c, 1);
            curBufPosition++;
            if (curBufPosition == maxBufSize) {
                maxBufSize *= 2;
                buf = realloc(buf, maxBufSize);
            }
            if (i < (int) (sizeof(char) * fileSize) - 1) {
                continue;
            }
        }
        if (isspace(c) || i == sizeof(char) * fileSize - 1) {
            file->wordCount += 1;
            if (curBufPosition > 0) {
                buf[curBufPosition] = '\0';
                if (file->words == NULL) {
                    file->words = malloc(sizeof(struct wordNode));
                    file->words->word = malloc(strlen(buf) + 1);
                    strcpy(file->words->word, buf);
                    file->words->numWords = 1;
                    file->words->next = NULL;
                    file->words->dProb = 0;
                } else {
                    struct wordNode *ptr = file->words;
                    int found = 0;
                    while (ptr != NULL) {
                        if (strcmp(ptr->word, buf) == 0) {
                            found = 1;
                            ptr->numWords += 1;
                            break;
                        }
                        ptr = ptr->next;
                    }
                    if (!found) {
                        struct wordNode *newWord = malloc(sizeof(struct wordNode));
                        newWord->numWords = 1;
                        newWord->word = malloc(strlen(buf) + 1);
                        strcpy(newWord->word, buf);
                        newWord->dProb = 0;
                        newWord->next = NULL;
                        struct wordNode *curr = file->words;
                        struct wordNode *prev = NULL;

                        while (curr != NULL && strcmp(curr->word, newWord->word) <= 0) {
                            prev = curr;
                            curr = curr->next;
                        }

                        if (prev == NULL) {
                            newWord->next = curr;
                            file->words = newWord;
                        } else {
                            prev->next = newWord;
                            newWord->next = curr;
                        }
                    }
                }
                free(buf);
                buf = NULL;
                curBufPosition = 0;
                buf = malloc(maxBufSize);
            }
        }
    }
    struct wordNode *ptr = file->words;
    while (ptr != NULL) {
        ptr->dProb = ptr->numWords / file->wordCount;
        ptr = ptr->next;
    }



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
    DIR *currDir = opendir(dirPath);
    if (currDir == NULL) {
        printf("Directory %s is inaccessible. Returning...\n", dirPath);
        return NULL;
    }
    pthread_t *pThreads = malloc(sizeof(pthread_t) * 10);//need to track all pthreads for joining
    int numPThreads = 10;
    int currPThread = 0;
    while (currDir != NULL) {
        errno = 0;
        struct dirent *entry = readdir(currDir);
        if (entry == NULL && errno == -1) {//file that can't be accessed will not terminate program
            printf("Directory entry is inaccessible. Continuing...\n");
            continue;
        }
        if (entry == NULL) {
            break;
        }
        if (entry->d_type == DT_DIR && (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)) {
            continue;
        }
        if (entry->d_type == DT_REG || entry->d_type == DT_DIR) {
            pthread_t thread;
            if (currPThread == numPThreads) {//dynamically resizing pthread array
                numPThreads *= 2;
                pThreads = realloc(pThreads, sizeof(pthread_t) * numPThreads);
            }
            struct arguments *newArgs = malloc(sizeof(struct arguments));
            char *newPathName = malloc(strlen(args->pathName) + strlen(entry->d_name) + 2);
            strcpy(newPathName, args->pathName);
            strcat(newPathName, entry->d_name);
            if (entry->d_type == DT_DIR)
                strcat(newPathName, "/");
            strcat(newPathName, "\0");
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
    strcat(directory, "/\0");//adding / to end of directory ensures it will be read
    if (opendir(directory) == NULL) {
        printf("Initial directory is inaccessible. Terminating. \n");
        return EXIT_FAILURE;
    }

    struct fileNode *head = malloc(sizeof(struct fileNode));
    head->wordCount = -1;
    pthread_mutex_t lock;
    struct arguments *args = malloc(sizeof(struct arguments));
    args->pathName = malloc(strlen(directory) + 1);
    strcpy(args->pathName, directory);
    args->head = head;
    args->lock = &lock;
    handleDirectory(args);
    struct fileNode *ptr = args->head;
    while (ptr != NULL) {
        printf("File: %s with %f words-- Words: ", ptr->fileName, ptr->wordCount);
        struct wordNode *wordPtr = ptr->words;
        while (wordPtr != NULL) {
            printf("[\"%s\" count = %f  dprob = %f] -> ", wordPtr->word, wordPtr->numWords, wordPtr->dProb);
            wordPtr = wordPtr->next;
        }
        printf("NULL\n|\n|\n");
        ptr = ptr->next;
    }
    printf("NULL\n");
    return 0;
}
