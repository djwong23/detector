#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>

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


struct wordNode* insertToken(struct wordNode *tokens, struct wordNode* iToken);
struct finalValNode* insertFinalValNode(struct finalValNode *head, struct finalValNode *insertNode);
struct finalValNode* jsd(struct fileNode* file1, struct fileNode* file2, struct finalValNode* head);

int main( int argc, char **argv) {
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


    //A.TXT
    struct fileNode* files = (struct fileNode*) malloc(sizeof(struct fileNode));         

    files->fileName = "a.txt";
    files->wordCount=4;
    files->next = NULL;

    struct wordNode* wordA1 = (struct wordNode*) malloc(sizeof(struct wordNode));
    wordA1->word="hi";
    wordA1->numWords=2;
    wordA1->dProb=0.5;

    struct wordNode* wordA2 = (struct wordNode*) malloc(sizeof(struct wordNode));
    wordA2->word="there";
    wordA2->numWords=2;
    wordA2->dProb=0.5;
    wordA2->next = NULL;

    wordA1->next = wordA2;
    files->words=wordA1;

    //B.TXT
    struct fileNode* bfile = (struct fileNode*) malloc(sizeof(struct fileNode));         

    bfile->fileName = "b.txt";
    bfile->wordCount=4;
    bfile->next = NULL;

    struct wordNode* wordb1 = (struct wordNode*) malloc(sizeof(struct wordNode));
    wordb1->word="hi";
    wordb1->numWords=2;
    wordb1->dProb=0.5;

    struct wordNode* wordb2 = (struct wordNode*) malloc(sizeof(struct wordNode));
    wordb2->word="out";
    wordb2->numWords=1;
    wordb2->dProb=0.25;
    wordb2->next = NULL;

    struct wordNode* wordb3 = (struct wordNode*) malloc(sizeof(struct wordNode));
    wordb3->word="there";
    wordb3->numWords=1;
    wordb3->dProb=0.25;
    wordb3->next = NULL;

    wordb1->next = wordb2;
    wordb2->next = wordb3;
    bfile->words=wordb1;

    files->next=bfile;


    //C.TXT
    struct fileNode* cfile = (struct fileNode*) malloc(sizeof(struct fileNode));         

    cfile->fileName = "c.txt";
    cfile->wordCount=9;
    cfile->next = NULL;

    struct wordNode* wordc1 = (struct wordNode*) malloc(sizeof(struct wordNode));
    wordc1->word="hi";
    wordc1->numWords=2;
    wordc1->dProb=(2.0/9);

    struct wordNode* wordc2 = (struct wordNode*) malloc(sizeof(struct wordNode));
    wordc2->word="jane";
    wordc2->numWords=3;
    wordc2->dProb=(3/9.0);
    wordc2->next = NULL;

    struct wordNode* wordc3 = (struct wordNode*) malloc(sizeof(struct wordNode));
    wordc3->word="jello";
    wordc3->numWords=4;
    wordc3->dProb=(4/9.0);
    wordc3->next = NULL;

    wordc1->next = wordc2;
    wordc2->next = wordc3;
    cfile->words=wordc1;

    bfile->next=cfile;

    struct fileNode* p = files;
    while(p!=NULL){
        printf("FileName: %s\n", p->fileName);
        struct wordNode* pw = p->words;
        while(pw!=NULL){
            printf("\t%s %lf %lf\n", pw->word, pw->numWords, pw->dProb);
            pw=pw->next;
        }
        p = p->next;
    }

    struct finalValNode* finalValHead = NULL;       //contains all final KLD/JSD values

    struct fileNode* ptrFile1 = files;      //pointers for nested loop
    struct fileNode * ptrFile2;             //pointers for nested loop

    while(ptrFile1!= NULL){
        ptrFile2 = ptrFile1->next;
        while(ptrFile2 != NULL){
            // printf("Comparing %s and %s\n", ptrFile1->fileName, ptrFile2->fileName);
            finalValHead = jsd(ptrFile1, ptrFile2, finalValHead);
            ptrFile2 = ptrFile2->next;
        }
        ptrFile1 = ptrFile1->next;
    }

    //RESULTS 
    struct finalValNode* finalPtr = finalValHead;
    while(finalPtr != NULL){
        printf("%s %s %lf\n", finalPtr->firstName, finalPtr->secondName, finalPtr->jsd);
        finalPtr = finalPtr->next;
    }
    



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
        // printf("%s: %lf %lf %lf\n", currNode->token, currNode->firstProb, currNode->secondProb, currNode->meanProb);        
        if(currNode->firstProb != 0){
            double x = currNode->firstProb * (log((currNode->firstProb/currNode->meanProb))/log(10));
            kld1 += x;
        }
        if(currNode->secondProb != 0){
            double x = currNode->secondProb * (log((currNode->secondProb/currNode->meanProb))/log(10));
            kld2 += x;
        }
        currNode = currNode->next;
        numTokensCombined++;
    }

    struct finalValNode* res = (struct finalValNode*) malloc(sizeof(struct finalValNode));
    res->firstName = file1->fileName;
    res->secondName = file2->fileName;
    res->numTokens = numTokensCombined; 
    res->jsd = (kld1+kld2)/2;
    res->next=NULL;
    
    return insertFinalValNode(head, res);
}


struct finalValNode* insertFinalValNode(struct finalValNode *head, struct finalValNode *insertNode){
    struct finalValNode* currNode = head;
    struct finalValNode* prevNode = NULL;
    int insertNumTokens = insertNode->numTokens;
    while(currNode != NULL && insertNumTokens >= currNode->numTokens){
        prevNode = currNode;
        currNode = currNode->next;
    }

    if(prevNode == NULL){
        insertNode->next = currNode;
        return insertNode;
    }

    prevNode->next = insertNode;
    insertNode->next = currNode;
    return head;
}

struct wordNode* insertToken(struct wordNode *tokens, struct wordNode* iToken){
    struct wordNode* curr = tokens;
    struct wordNode* prev = NULL;
    
    while(curr != NULL && strcmp(curr->word, iToken->word) <= 0){
        prev = curr;
        curr=curr->next;
    }

    if(prev == NULL){
        iToken->next = curr;
        return iToken;
    }
    prev->next = iToken;
    iToken->next=curr;
    return tokens;
}



