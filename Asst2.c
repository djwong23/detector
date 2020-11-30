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

void *handleFile(void *input);
void *handleDirectory(void *input);
struct wordNode *insertToken(struct wordNode *tokens, struct wordNode *iToken);
struct finalValNode *insertFinalValNode(struct finalValNode *head, struct finalValNode *insertNode);
struct finalValNode *jsd(struct fileNode *file1, struct fileNode *file2, struct finalValNode *head);
struct fileNode *sortSharedStructure(struct fileNode *head, struct fileNode *insert);
void printJSD(double j);

int main(int argc, char **argv) {
    char *directory = malloc(strlen(argv[1]) + 2);
    strcpy(directory, argv[1]);
    strcat(directory, "/\0");//adding / to end of directory ensures it will be read
    DIR *dirStream = opendir(directory);
    if (dirStream == NULL) {
        printf("Initial directory is inaccessible. Terminating. \n");
        return EXIT_FAILURE;
    }

    struct fileNode *head = malloc(sizeof(struct fileNode));
    head->wordCount = -1;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    struct arguments *args = malloc(sizeof(struct arguments));
    args->pathName = malloc(strlen(directory) + 1);
    strcpy(args->pathName, directory);
    args->head = head;
    args->lock = &lock;
    handleDirectory(args);


    // struct fileNode *ptr = args->head;
    // while (ptr != NULL) {
    //     printf("File: %s with %f words-- Words: ", ptr->fileName, ptr->wordCount);
    //     struct wordNode *wordPtr = ptr->words;
    //     while (wordPtr != NULL) {
    //         printf("[\"%s\" count = %f  dprob = %f] -> ", wordPtr->word, wordPtr->numWords, wordPtr->dProb);
    //         wordPtr = wordPtr->next;
    //     }
    //     printf("NULL\n|\n|\n");
    //     ptr = ptr->next;
    // }
    // printf("NULL\n");

    struct fileNode *sortedFiles = NULL;
    struct fileNode *sortingPtr = args->head;
    while (sortingPtr != NULL) {
        sortedFiles = sortSharedStructure(sortedFiles, sortingPtr);
        sortingPtr = sortingPtr->next;
    }

    // struct fileNode* t = sortedFiles;
    // while(t != NULL){
    //     printf("%s %lf\n", t->fileName, t->wordCount);
    //     t=t->next;
    // }


    struct finalValNode *finalValHead = NULL;

    struct fileNode *ptr1 = sortedFiles;
    struct fileNode *ptr2;

    while (ptr1 != NULL) {
        ptr2 = ptr1->next;
        while (ptr2 != NULL) {
            finalValHead = jsd(ptr1, ptr2, finalValHead);
            ptr2 = ptr2->next;
        }
        ptr1=ptr1->next;
    }

    struct finalValNode *res = finalValHead;

    while (res != NULL) {
        printJSD(res->jsd);
        printf("\"%s\" \"%s\"\n", res->firstName, res->secondName);
        res = res->next;
    }

    struct fileNode* ptr = head;
    struct fileNode *prevFilePtr = NULL;
    while (ptr != NULL) {
        struct wordNode *wordPtr = ptr->words;
        struct wordNode *prevPtr = NULL;
        while (wordPtr != NULL) {
            prevPtr = wordPtr;
            wordPtr = wordPtr->next;
            free(prevPtr->word);
            prevPtr->word = NULL;
            free(prevPtr);
            prevPtr = NULL;
        }
        prevFilePtr = ptr;
        ptr = ptr->next;
        free(prevFilePtr->fileName);
        prevFilePtr->fileName = NULL;
        free(prevFilePtr);
        prevFilePtr = NULL;
    }
    struct finalValNode* freeFinalPtr = finalValHead;
    struct finalValNode *prevValPtr = NULL;
    while (freeFinalPtr != NULL) {
        prevValPtr = freeFinalPtr;
        freeFinalPtr = freeFinalPtr->next;
        free(prevValPtr);
        prevValPtr = NULL;
    }
    free(directory);
    closedir(dirStream);
    return 0;
}

void printJSD(double j) {
    /*red for [0, 0.1]
yellow for (0.1, 0.15]
green for (0.15, 0.2]
cyan for (0.2, 0.25]
blue for (0.25, 0.3]
and white for any greater than 0.3.*/
    if (j >= 0 && j <= 0.1) {
        printf("\033[1;31m");
    } else if (j > 0.1 && j <= 0.15) {
        printf("\033[1;33m");
    } else if (j > 0.15 && j <= 0.2) {
        printf("\033[0;32m");
    } else if (j > 0.2 && j <= 0.25) {
        printf("\033[0;36m");
    } else if (j > 0.25 && j <= 0.3) {
        printf("\033[0;34m");
    }
    printf("%lf ", j);
    printf("\033[0m");
}

struct finalValNode *jsd(struct fileNode *file1, struct fileNode *file2, struct finalValNode *head) {
    struct JSDNode *combined = NULL;
    struct JSDNode *currLocation = NULL;
    struct wordNode *ptr1 = file1->words;
    struct wordNode *ptr2 = file2->words;

    while (ptr1 != NULL && ptr2 != NULL) {
        int cmp = strcmp(ptr1->word, ptr2->word);
        struct JSDNode *temp = (struct JSDNode *) malloc(sizeof(struct JSDNode));
        if (cmp == 0) {
            temp->token = ptr1->word;
            temp->firstProb = ptr1->dProb;
            temp->secondProb = ptr2->dProb;
            temp->meanProb = ((temp->firstProb) + (temp->secondProb)) / 2;
            temp->next = NULL;
            ptr1 = ptr1->next;
            ptr2 = ptr2->next;
        } else if (cmp > 0) {
            temp->token = ptr2->word;
            temp->firstProb = 0;
            temp->secondProb = ptr2->dProb;
            temp->meanProb = (0 + (temp->secondProb)) / 2;
            temp->next = NULL;
            ptr2 = ptr2->next;
        } else {
            temp->token = ptr1->word;
            temp->firstProb = ptr1->dProb;
            temp->secondProb = 0;
            temp->next = NULL;
            temp->meanProb = (0 + (temp->firstProb)) / 2;
            ptr1 = ptr1->next;
        }

        if (combined == NULL) {
            combined = temp;
            currLocation = combined;
        } else {
            currLocation->next = temp;
            currLocation = currLocation->next;
        }
    }

    if (ptr1 == NULL && ptr2 != NULL) {
        while (ptr2 != NULL) {
            struct JSDNode *temp = (struct JSDNode *) malloc(sizeof(struct JSDNode));
            temp->token = ptr2->word;
            temp->firstProb = 0;
            temp->secondProb = ptr2->dProb;
            temp->next = NULL;
            temp->meanProb = (0 + (temp->secondProb)) / 2;
            ptr2 = ptr2->next;
            if (combined == NULL) {
                combined = temp;
                currLocation = combined;
            } else {
                currLocation->next = temp;
                currLocation = currLocation->next;
            }
        }
    } else if (ptr1 != NULL && ptr2 == NULL) {
        while (ptr1 != NULL) {
            struct JSDNode *temp = (struct JSDNode *) malloc(sizeof(struct JSDNode));
            temp->token = ptr1->word;
            temp->firstProb = ptr1->dProb;
            temp->next = NULL;
            temp->secondProb = 0;
            temp->meanProb = (0 + (temp->firstProb)) / 2;
            ptr1 = ptr1->next;
            if (combined == NULL) {
                combined = temp;
                currLocation = combined;
            } else {
                currLocation->next = temp;
                currLocation = currLocation->next;
            }
        }
    }

    struct JSDNode *currNode = combined;

    double kld1 = 0;
    double kld2 = 0;
    int numTokensCombined = 0;

    while (currNode != NULL) {
        if (currNode->firstProb != 0) {
            double x = currNode->firstProb * (log((currNode->firstProb / currNode->meanProb)) / log(10));
            kld1 += x;
        }
        if (currNode->secondProb != 0) {
            double x = currNode->secondProb * (log((currNode->secondProb / currNode->meanProb)) / log(10));
            kld2 += x;
        }
        currNode = currNode->next;
        numTokensCombined++;
    }

    struct JSDNode *freePtr = combined;
    struct JSDNode *nextFreePtr = combined;
    while(freePtr != NULL){
        nextFreePtr=freePtr->next;
        free(freePtr);
        freePtr = nextFreePtr;
    }


    struct finalValNode *res = (struct finalValNode *) malloc(sizeof(struct finalValNode));
    res->firstName = file1->fileName;
    res->secondName = file2->fileName;
    res->numTokens = numTokensCombined;
    res->jsd = (kld1 + kld2) / 2;
    res->next = NULL;
    
    return insertFinalValNode(head, res);
}


struct finalValNode *insertFinalValNode(struct finalValNode *head, struct finalValNode *insertNode) {
    struct finalValNode *currNode = head;
    struct finalValNode *prevNode = NULL;
    int insertNumTokens = insertNode->numTokens;
    while (currNode != NULL && insertNumTokens >= currNode->numTokens) {
        prevNode = currNode;
        currNode = currNode->next;
    }

    if (prevNode == NULL) {
        insertNode->next = currNode;
        return insertNode;
    }

    prevNode->next = insertNode;
    insertNode->next = currNode;
    return head;
}


void *handleFile(void *input) {
    struct arguments *args = (struct arguments *) input;
    printf("%s has been accessed\n", args->pathName);
    int fileDesc = open(args->pathName, O_RDONLY);
    if (fileDesc == -1) {
        printf("File %s is inaccessible. Returning...\n", args->pathName);
        return NULL;
    }

    pthread_mutex_lock(args->lock);
    struct fileNode *file = NULL;
    struct fileNode *head = args->head;
    if (head->wordCount == -1) {
        file = head;
    } else {
        file = malloc(sizeof(struct fileNode));
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
    free(buf);
    buf = NULL;
    close(fileDesc);
    struct wordNode *ptr = file->words;
    while (ptr != NULL) {
        ptr->dProb = ptr->numWords / file->wordCount;
        ptr = ptr->next;
    }
    free(args->pathName);
    free(args);


    //  printf("This is %s\n", args->pathName);
    return NULL;
}

void *handleDirectory(void *input) {
    struct arguments *args = (struct arguments *) input;
    char *dirPath = args->pathName;
    DIR *currDir = opendir(dirPath);
    if (currDir == NULL) {
        printf("Directory %s is inaccessible. Returning...\n", dirPath);
        return NULL;
    }
    int numPThreads = 10;
    int currPThread = 0;
    pthread_t *pThreads = malloc(sizeof(pthread_t) * numPThreads);//need to track all pthreads for joining
    struct arguments *newArgs = NULL;


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
            if (currPThread == numPThreads) {//dynamically resizing pthread array
                numPThreads *= 2;
                pThreads = realloc(pThreads, sizeof(pthread_t) * numPThreads);
            }
            pthread_t thread;
            newArgs = malloc(sizeof(struct arguments));
            char *newPathName = malloc(strlen(args->pathName) + strlen(entry->d_name) + 2);
            strcpy(newPathName, args->pathName);
            strcat(newPathName, entry->d_name);
            if (entry->d_type == DT_DIR)
                strcat(newPathName, "/");
            strcat(newPathName, "\0");
            newArgs->pathName = malloc(strlen(newPathName) + 1);
            strcpy(newArgs->pathName, newPathName);
            free(newPathName);
            newPathName = NULL;
            newArgs->head = args->head;
            newArgs->lock = args->lock;
            pThreads[currPThread] = thread;
            int ret;
            if (entry->d_type == DT_DIR)
                ret = pthread_create(&pThreads[currPThread], NULL, handleDirectory, newArgs);
            else
                ret = pthread_create(&pThreads[currPThread], NULL, handleFile, newArgs);
            if (ret != 0) {
                printf("Error while creating pthread\n");
            }
            currPThread++;
        }
    }
    closedir(currDir);

    for (int i = 0; i < currPThread; i++) {
        int r = pthread_join(pThreads[i], NULL);
        if (r != 0) {
            printf("An error has ocurred while joining threads at %s \n", dirPath);
            return NULL;
        }
    }
    free(pThreads);
    free(args->pathName);
    free(args);
    return NULL;
}

struct fileNode *sortSharedStructure(struct fileNode *head, struct fileNode *insert) {
    struct fileNode *temp = (struct fileNode *) malloc(sizeof(struct fileNode));
    temp->fileName = insert->fileName;
    temp->wordCount = insert->wordCount;
    temp->words = insert->words;
    temp->next = NULL;

    if (head == NULL) {
        return temp;
    }

    struct fileNode *prevPtr = NULL;
    struct fileNode *currPtr = head;
    double insertNumTokens = insert->wordCount;

    while (currPtr != NULL && insertNumTokens >= currPtr->wordCount) {
        prevPtr = currPtr;
        currPtr = currPtr->next;
    }
    if (prevPtr == NULL) {
        temp->next = currPtr;
        return temp;
    }
    prevPtr->next = temp;
    temp->next = currPtr;
    return head;
}

