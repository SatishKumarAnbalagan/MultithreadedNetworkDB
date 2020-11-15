
#include <stdio.h>
#include <stdlib.h>

typedef struct qNode {
    int val;
    struct qNode* next;
} qNode_t;

typedef struct queue {
    qNode_t* head;
    qNode_t* tail;
    unsigned int size;
} queue_t;

void enqueue(queue_t* queue, int val);
int dequeue(queue_t* queue);
void print_queue(queue_t* queue);
queue_t* create_queue();
void free_queue(queue_t* queue);