#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
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
    struct JSDNode *next;
};

struct finalValNode {
    char *firstName;
    char *secondName;
    double jsd;
    int numTokens;
    struct finalValNode *next;
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
void handleFile(struct arguments *args);

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
        } else if (entry == NULL) {
            printf("End of directory %s reached.\n", dirPath);
            break;

        } else if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
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
            pthread_create(&thread, NULL, handleDirectory, newArgs);
            pThreads[currPThread++] = thread;
        } else {
            printf("%s\n", entry->d_name);
        }

    }
    int i;
    printf("Joining threads...\n");
    for (i = 0; i < currPThread; i++) {
        pthread_join(pThreads[i], NULL);
    }
    return NULL;
}


//ARCHI'S TO DO LIST

//an insertion function to insert words in appropriate location alphabetically in linked list

//jsd function


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

struct wordNode* insertToken(struct wordNode *tokens);
struct finalValNode* insertFinalValNode(struct finalValNode *currNode);
struct finalValNode* jsd(struct fileNode* file1, struct fileNode* file2, struct finalValNode* head);

int main(int argc, char **argv) {
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
    struct fileNode* files = NULL;          //shared data structure from file handlings

    struct finalValNode* finalValHead = NULL;       //contains all final KLD/JSD values

    struct fileNode* ptrFile1 = files;      //pointers for nested loop
    struct fileNode * ptrFile2;             //pointers for nested loop

    while(ptrFile1!= NULL){
        ptrFile2 = ptrFile1->next;
        while(ptrFile2 != NULL){
            finalValHead = jsd(ptrFile1, ptrFile2, finalValHead);
            ptrFile2 = ptrFile2->next;
        }
        ptrFile1 = ptrFile1->next;
    }
    


    return 0;
}

struct finalValNode* jsd(struct fileNode* file1, struct fileNode* file2, struct finalValNode* head){
    struct JSDNode* combined = NULL;
    struct JSDNode* currLocation = NULL;
    struct wordNode* ptr1 = file1->words;
    struct wordNode* ptr2 =  file2->words;

    while(ptr1 != NULL && ptr2 != NULL){
        int cmp = strcmp(ptr1->word, ptr2->word);
        struct JSDNode* temp = (struct JSDNode*) malloc(sizeof(struct wordNode));
        if(cmp == 0){
            temp->token = ptr1->word;
            temp->firstProb = ptr1->dProb;
            temp->secondProb = ptr2->dProb;
            temp->meanProb = ((temp->firstProb) + (temp->secondProb))/2;

            ptr1 = ptr1->next;
            ptr2 = ptr2->next;
        }
        else if(cmp > 0){
            temp->token = ptr2->word;
            temp->firstProb = 0;
            temp->secondProb = ptr2->dProb;
            temp->meanProb = (0+(temp->secondProb))/2;
            ptr2 = ptr2->next;
        }
        else{
            temp->token = ptr1->word;
            temp->firstProb = ptr1->dProb;
            temp->secondProb = 0;
            temp->meanProb = (0+(temp->firstProb))/2;
            ptr1 = ptr1->next;
        }

        if(combined == NULL){
            combined = temp;
            currLocation= combined;
        }
        else{
            currLocation->next = temp;
            currLocation = currLocation->next;
        }

    }

    if(ptr1 == NULL && ptr2 != NULL){
        while(ptr2!=NULL){
            struct JSDNode* temp = (struct JSDNode*) malloc(sizeof(struct wordNode));
            temp->token = ptr2->word;
            temp->firstProb = 0;
            temp->secondProb = ptr2->dProb;
            temp->meanProb = (0+(temp->secondProb))/2;
            ptr2 = ptr2->next;
            if(combined == NULL){
                combined = temp;
                currLocation= combined;
            }
            else{
                currLocation->next = temp;
                currLocation = currLocation->next;
            }
        }
    }
    else if (ptr1 != NULL && ptr2 == NULL){
        while(ptr1!=NULL){
            struct JSDNode* temp = (struct JSDNode*) malloc(sizeof(struct wordNode));
            temp->token = ptr1->word;
            temp->firstProb = ptr1->dProb;
            temp->secondProb = 0;
            temp->meanProb = (0+(temp->firstProb))/2;
            ptr1 = ptr1->next;
            if(combined == NULL){
                combined = temp;
                currLocation= combined;
            }
            else{
                currLocation->next = temp;
                currLocation = currLocation->next;
            }
        }
    }

    struct JSDNode* currNode = combined;

    double kld1 = 0;
    double kld2 = 0;
    int numTokensCombined = 0;

    while(currNode != NULL){
        kld1 += currNode->firstProb * log(currNode->firstProb/currLocation->meanProb);
        kld1 += currNode->secondProb * log(currNode->secondProb/currLocation->meanProb);
        currNode = currNode->next;
        numTokensCombined++;
    }

    // int x = log(100);

    struct finalValNode* res = (struct finalValNode*) malloc(sizeof(struct finalValNode));
    res->firstName = file1->fileName;
    res->secondName = file2->fileName;
    res->numTokens = numTokensCombined; 
    res->jsd = (kld1+kld2)/2;
    

    return head;
}

