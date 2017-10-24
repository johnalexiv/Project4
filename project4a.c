//
//  project4a.c
//  Project4
//
//  Created by John Alexander on 10/17/17.
//  Copyright © 2017 John Alexander. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

struct QueueNode 
{
    int value;
    struct QueueNode *next;
};

struct Queue
{
    struct QueueNode *front;
    struct QueueNode *rear;
};

struct Channel
{
    struct Queue *queue;
    sem_t syncSemaphore;
    sem_t raceSemaphore;
};

struct Node
{
    int uid;
    int tempUniqueID;
    int oneHopID;
    int twoHopID;
    bool status;
    bool leader;
    pthread_t thread;
};

struct NodeInfo
{
    int id;
    int size;
    bool *leaderFound;
    struct Channel *channels;
    struct Node node;
};

int getIdCount();
int *getInputFromUser(int);
struct Channel *createChannels(int);
struct Node *createNodes(int [], int);
struct NodeInfo *createNodeInfo(struct Node *, struct Channel *, bool *, int);
void runThreads(struct NodeInfo *, int);
void waitForThreads(struct NodeInfo *, int);
void runThread(void *);
void activeNode(struct NodeInfo *, bool *, int);
void relayNode(struct NodeInfo *, bool *);
void foundLeader(struct NodeInfo *, bool *);
void writeToChannel(struct NodeInfo *, int);
void write(struct Channel *, int);
int readFromChannel(struct NodeInfo *);
int read(struct Channel *);
void printInfo(struct NodeInfo *, int);

struct Queue *createQueue();
int qFront(struct Queue *);
void qInsert(struct Queue *, int);
struct QueueNode *newNode(int);
void qRemove(struct Queue *);

int main()
{
    int idCount = getIdCount();
    int *uniqueIDs = getInputFromUser(idCount);

    struct Channel *channels = createChannels(idCount);

    struct Node *nodes = createNodes(uniqueIDs, idCount);

    bool *leaderFound = (bool *)malloc(sizeof(bool));
    struct NodeInfo *nodeInfo = createNodeInfo(nodes, channels, leaderFound, idCount);

    runThreads(nodeInfo, idCount);

    waitForThreads(nodeInfo, idCount);

    return 0;
}

int getIdCount()
{
    int idCount;
    scanf("%d", &idCount);
    return idCount;
}

int *getInputFromUser(int count)
{
    int num;
    int index = 0;
    int *uniqueIDs = (int *)calloc(count, sizeof(int *));
    while ( scanf("%d", &num) == 1 )
        uniqueIDs[index++] = num;

    return uniqueIDs;    
}

struct Channel *createChannels(int idCount)
{   
    int i;
    struct Channel *channels = (struct Channel *)calloc(idCount, sizeof(struct Channel));

    for ( i = 0; i < idCount; i++ )
    {
        channels[i].queue = createQueue();
        sem_init(&channels[i].raceSemaphore, 0, 1);
        sem_init(&channels[i].syncSemaphore, 0, 0);
    }

    return channels;
}

struct Node *createNodes(int uniqueIDs[], int idCount)
{
    int i;
    struct Node *nodes = (struct Node *)calloc(idCount, sizeof(struct Node));

    for ( i = 0; i < idCount; i++ )
    {
        nodes[i].uid = nodes[i].tempUniqueID = uniqueIDs[i];
        nodes[i].status = true;
        nodes[i].leader = false;
        pthread_t thread;
        nodes[i].thread = thread;
    }

    return nodes;
}

struct NodeInfo *createNodeInfo(struct Node *nodes, struct Channel *channels, bool *leaderFound, int idCount)
{
    int i;
    struct NodeInfo *nodeInfo = (struct NodeInfo *)calloc(idCount, sizeof(struct NodeInfo));
    *leaderFound = false;

    for ( i = 0; i < idCount; i++ )
    {
        nodeInfo[i].id = i;
        nodeInfo[i].size = idCount;
        nodeInfo[i].leaderFound = leaderFound;
        nodeInfo[i].node = nodes[i];
        nodeInfo[i].channels = channels;
    }

    return nodeInfo;
}

void runThreads(struct NodeInfo *nodeInfo, int idCount)
{
    if ( nodeInfo == NULL )
        return;

    int i;
    for ( i = 0; i < idCount; i++ )
    {
        pthread_create(&nodeInfo[i].node.thread, NULL, (void *)runThread, &nodeInfo[i]);
    }
}

void runThread(void *arugments)
{
    struct NodeInfo *nodeInfo = (struct NodeInfo *)arugments;

    int phase = 1;
    bool *leaderFound = nodeInfo->leaderFound;

    while ( !*leaderFound )
    {
        if ( nodeInfo->node.status )
            activeNode(nodeInfo, leaderFound, phase);
        else
            relayNode(nodeInfo, leaderFound);

        phase++;
    }

    pthread_exit(NULL);
}

void activeNode(struct NodeInfo *nodeInfo, bool *leaderFound, int phase)
{
    printInfo(nodeInfo, phase);

    writeToChannel(nodeInfo, nodeInfo->node.tempUniqueID);
    nodeInfo->node.oneHopID = readFromChannel(nodeInfo);
    writeToChannel(nodeInfo, nodeInfo->node.oneHopID);
    nodeInfo->node.twoHopID = readFromChannel(nodeInfo);

    if ( nodeInfo->node.oneHopID == nodeInfo->node.tempUniqueID )
        foundLeader(nodeInfo, leaderFound);

    else if ( nodeInfo->node.oneHopID > nodeInfo->node.twoHopID &&
            nodeInfo->node.oneHopID > nodeInfo->node.tempUniqueID )
        nodeInfo->node.tempUniqueID = nodeInfo->node.oneHopID;

    else
        nodeInfo->node.status = false;
}

void printInfo(struct NodeInfo *nodeInfo, int phase)
{
    printf("[%d]", phase);
    printf("[%d]", nodeInfo->node.uid);
    printf("[%d]\n", nodeInfo->node.tempUniqueID);
}

void foundLeader(struct NodeInfo *nodeInfo, bool *leaderFound)
{
    *leaderFound = true;
    nodeInfo->node.leader = true;
    writeToChannel(nodeInfo, -1);
    writeToChannel(nodeInfo, -1);
}

void relayNode(struct NodeInfo *nodeInfo, bool *leaderFound)
{
    if ( nodeInfo->node.tempUniqueID == -1 )
        return;

    int tempuid;
    tempuid = readFromChannel(nodeInfo);
    writeToChannel(nodeInfo, tempuid);
    tempuid = readFromChannel(nodeInfo);
    writeToChannel(nodeInfo, tempuid);
}

void waitForThreads(struct NodeInfo *nodeInfo, int idCount)
{
    int i;
    for ( i = 0; i < idCount; i++ )
    {
        if ( nodeInfo[i].node.leader )
            printf("leader: %d\n", nodeInfo[i].node.uid);

        pthread_join(nodeInfo[i].node.thread, NULL);
    }
}

void writeToChannel(struct NodeInfo *nodeInfo, int value)
{
    write(&nodeInfo->channels[nodeInfo->id], value);
}

void write(struct Channel *channel, int value)
{
    sem_wait(&channel->raceSemaphore);
    qInsert(channel->queue, value);
    sem_post(&channel->raceSemaphore);
    sem_post(&channel->syncSemaphore);
}

int readFromChannel(struct NodeInfo *nodeInfo)
{
    int value;
    if ( nodeInfo->id == 0 )
        value = read(&nodeInfo->channels[nodeInfo->size - 1]);
    else
        value = read(&nodeInfo->channels[nodeInfo->id - 1]);

    return value;
}   

int read(struct Channel *channel)
{
    int value;
    sem_wait(&channel->syncSemaphore);
    sem_wait(&channel->raceSemaphore);
    value = qFront(channel->queue);
    qRemove(channel->queue);
    sem_post(&channel->raceSemaphore);

    return value;
}

struct Queue *createQueue()
{
    struct Queue *queue = (struct Queue*)malloc(sizeof(struct Queue));
    queue->front = NULL;
    queue->rear = queue->front;
    return queue;
}

int qFront(struct Queue *queue)
{
    if ( queue->front == NULL )
        return -1;

    return queue->front->value;
}

void qInsert(struct Queue *queue, int value)
{
    struct QueueNode *node = newNode(value);

    if ( queue->rear == NULL )
    {
        queue->front = queue->rear = node;
        return;
    }

    queue->rear->next = node;
    queue->rear = node;
}

void qRemove(struct Queue *queue)
{
    if ( queue->front == NULL )
        return;

    struct QueueNode *tempNode = queue->front;
    queue->front = queue->front->next;

    if ( queue->front == NULL )
        queue->rear = NULL;
}

struct QueueNode *newNode(int value)
{
    struct QueueNode *tempNode = (struct QueueNode*)malloc(sizeof(struct QueueNode));
    tempNode->value = value;
    tempNode->next = NULL;
    return tempNode;
}









