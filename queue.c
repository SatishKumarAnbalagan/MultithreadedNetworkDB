#include "queue.h"

void enqueue(queue_t* queue, int val) {
    qNode_t* newNode = (qNode_t*)malloc(sizeof(qNode_t));
    newNode->next = NULL;
    newNode->val = val;
    if (queue->tail == NULL) {
        queue->head = newNode;
        queue->tail = newNode;
    } else {
        queue->tail->next = newNode;
        queue->tail = newNode;
    }
    queue->size++;
}

int dequeue(queue_t* queue) {
    if (queue->head == NULL) {
        fprintf(stderr, "Error: try to dequeue empty queue.");
        return -1;
    }
    qNode_t* tmp = queue->head;
    queue->head = queue->head->next;

    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    int val = tmp->val;
    free(tmp);
    queue->size--;
    return val;
}

void print_queue(queue_t* queue) {
    qNode_t* curr = queue->head;
    while (curr != NULL) {
        fprintf(stderr, "%d ", curr->val);
        curr = curr->next;
    }
}

queue_t* create_queue() {
    queue_t* q = malloc(sizeof(queue_t));
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
    return q;
}

void free_queue(queue_t* queue) {
    qNode_t* curr = queue->head;
    while (curr != NULL) {
        qNode_t * next = curr->next;
        free(curr);
        curr = next;
    }
    free(queue);
}

// int main() {
//     queue_t* q = create_queue();
//     enqueue(q, 10);
//     print_queue(q);
//     enqueue(q, 20);
//     print_queue(q);

//     dequeue(q);
//     print_queue(q);

//     dequeue(q);
//     print_queue(q);

//     enqueue(q, 30);
//     print_queue(q);

//     enqueue(q, 40);
//     print_queue(q);
//     enqueue(q, 50);

//     printf("Queue Front : %d \n", q->head->val);
//     printf("Queue Rear : %d", q->tail->val);
//     free_queue(q);
//     return 0;
// }