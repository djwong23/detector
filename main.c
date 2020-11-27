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
void handleDirectory(struct arguments args) {
    char *dirPath = args.pathName;
    DIR *currDir = opendir(dirPath);
    struct dirent *entry = readdir(currDir);
    while (currDir != NULL) {

    }
}

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

int main(int argc, char **argv) {
    char *directory = malloc(sizeof(argv[1]) + 1);
    strcpy(directory, argv[1]);
    strcat(directory, "/");
    printf("%s\n", directory);
    if (opendir(directory) == NULL) {
        printf("Initial directory is inaccessible. Terminating. \n");
        return EXIT_FAILURE;
    }
    struct fileNode *head = malloc(sizeof(struct fileNode));
    pthread_mutex_t lock;
    struct arguments *args = malloc(sizeof(struct arguments));
    args->pathName = directory;
    args->head = head;
    args->lock = &lock;
    handleDirectory(*args);
    return 0;
}
