#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

// Macros
#define MAX 4096
#define MAX_STORE 200
#define FILE_NAME "./temp.txt"
#define READ_FLAG 'R'
#define WRITE_FLAG 'W'
#define DELETE_FLAG 'D'
#define OVERWRITE_FLAG 'O'
#define DB_DIR "./tmp"

#define KEY_INVALID 0
#define KEY_VALID 1
#define KEY_BUSY 2

#define OP_VALID 'S'
#define OP_INVALID 'I'
#define OP_BLOCKED 'B'
#define OP_EXCEED_STORE 'E'

// type definition
typedef struct stats_t {
    unsigned int table_items;
    unsigned int no_read_request;
    unsigned int no_write_request;
    unsigned int no_delete_request;
    unsigned int no_request_queued;
    unsigned int no_failed_request;
} stats_t;

typedef struct command_t {
    char op;
    char name[31];
    char len[8];
    char* data;
} command_t;

typedef struct command_reply_t {
    char status;
    char name[31];
    char len[8];
    char* data;
} command_reply_t;

typedef struct qNode {
    int val;
    struct qNode* next;
} qNode_t;

typedef struct queue {
    qNode_t* head;
    qNode_t* tail;
    unsigned int size;
} queue_t;

// global variable
stats_t* stats;
queue_t* job_queue;

pthread_mutex_t queue_cond_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t stat_mutex = PTHREAD_MUTEX_INITIALIZER;
char* keys[MAX_STORE];

int port = 5000;  // Default port

/**
 * @brief Implement R/W lock with counter.
 * read_status[i] == 0: invalid
 * read_status[i] == 1: valid
 * read_status[i] > 1: reading, each reading request increment read_status by 1.
 * lock write when reading.
 */
int read_status[MAX_STORE];
int write_status[MAX_STORE];

// function declaration

/**
 * @brief print queue content
 *
 * @param queue queue
 */
void print_queue(queue_t* queue);

/**
 * @brief Create a queue object on Heap with malloc
 *
 * @return queue_t* pointer to queue created.
 */
queue_t* create_queue();

/**
 * @brief enqueue an value,
 *
 * @param queue pointer to queue
 * @param val value
 */
void enqueue(queue_t* queue, int val);

/**
 * @brief dequeue an value.
 *
 * @param queue
 * @return int dequeued value
 */
int dequeue(queue_t* queue);

/**
 * @brief Free memory of queue including elements inside of queue.
 *
 * @param queue
 */
void free_queue(queue_t* queue);

/**
 * @brief listener thread function. establish listener on socket.
 *
 * @param ptr placeholder
 * @return void* placeholder
 */
void* listener(void* ptr);

/**
 * @brief worker thread function. It will wait on signal of job queue when new
 * job is received.
 *
 * @param worker_id worker thread id
 * @return void*
 */
void* handle_work(void* worker_id);

/**
 * @brief helper function to handle work
 *
 * @param socket
 * @return int
 */
int handle_work_helper(int socket);

/**
 * @brief
 *
 * @param sock_fd
 * @return int
 */
int queue_work(int sock_fd);

void prerequest_process(command_t* request, int* p_idx, char* p_status,
                        char* p_flag);
void postrequest_update(char flag, char status, int idx);

int get_work();
int read_data(char* filename, char* buf);
int write_data(char* filename, char* buf, int len);
void display_stats();

void init_global();

void handle_read(int socket, command_t* request, command_reply_t* reply,
                 int idx);
void handle_write(int socket, command_t* request, command_reply_t* reply,
                  int idx, char flag);
void handle_delete(int socket, command_t* request, command_reply_t* reply,
                   int idx);

/**
 * @brief search key in DB
 *
 * @param key DB key
 * @return int index if found, return -1 if not found.
 */
int search_key(char* key);

/**
 * @brief search for next free space in DB
 *
 * @return int index of free space else return -1;
 */
int search_free_entry();

/**
 * @brief generate database file path from DB index.
 *
 * @param path
 * @param index
 */
void create_path(char* path, int index);

void send_reply(int socket, command_reply_t* reply, char flag);
void update_stats(command_reply_t* reply, char flag);
void init_cmd(command_t* cmd);
void init_cmd_reply(command_reply_t* reply);

/**
 * @brief search key in DB
 *
 * @param key DB key
 * @return int index if found, return -1 if not found.
 */
int search_key(char* key);

/**
 * @brief search for next free space in DB
 *
 * @return int index of free space else return -1;
 */
int search_free_entry();

/**
 * @brief generate database file path from DB index.
 *
 * @param path
 * @param index
 */
void create_path(char* path, int index);

void send_reply(int socket, command_reply_t* reply, char flag);
void update_stats(command_reply_t* reply, char flag);
void init_cmd(command_t* cmd);
void init_cmd_reply(command_reply_t* reply);

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
        qNode_t* next = curr->next;
        free(curr);
        curr = next;
    }
    free(queue);
}

// function definition
void* listener(void* ptr) {
    int sock = socket(AF_INET, SOCK_STREAM, 0); /* (A) */
    struct sockaddr_in addr = {.sin_family = AF_INET,
                               .sin_port = htons(port), /* (B) */
                               .sin_addr.s_addr = 0};
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) /* (C) */
        perror("can't bind"), exit(1);
    if (listen(sock, 3) < 0) /* (D) */
        perror("listen"), exit(1);

    while (1) {
        int fd = accept(sock, NULL, NULL); /* (E) */

        if (fd < 0)
            perror("server acccept failed...\n"), exit(1);
        else
            perror("server acccept the client...\n");

        // printf("file descriptor: %d\n", fd);

        // usleep(random() % 10000);

        pthread_mutex_lock(&queue_cond_mutex);
        queue_work(fd);
        if (job_queue->size > 0) {
            pthread_cond_signal(&queue_cond);
        }
        pthread_mutex_unlock(&queue_cond_mutex);

        // handle_work(get_work());
    }

    return (void*)0;
}

void* handle_work(void* worker_id) {
    int* w_id = worker_id;

    for (;;) {
        pthread_mutex_lock(&queue_cond_mutex);
        fprintf(stderr, "worker id: %d; job queue size: %d\n", *w_id,
                job_queue->size);
        while (job_queue->size == 0) {
            pthread_cond_wait(&queue_cond, &queue_cond_mutex);
        }
        int sock_fd = get_work();

        pthread_mutex_unlock(&queue_cond_mutex);
        handle_work_helper(sock_fd);
    }

    return 0;
}

int handle_work_helper(int sock_fd) {
    // read request message to request struct. Excluding data of request
    // which will be read later by handle_write.
    command_t* request = malloc(sizeof(*request));
    init_cmd(request);
    read(sock_fd, &(request->op), sizeof(request->op));
    read(sock_fd, &(request->name), sizeof(request->name));
    read(sock_fd, &(request->len), sizeof(request->len));

    fprintf(stderr, "Request:\n");
    fprintf(stderr, "Op: %c \t name: %s \t len: %d\n", request->op,
            request->name, atoi(request->len));

    char flag = request->op;
    char status = OP_VALID;
    int idx = -1;
    if (flag == WRITE_FLAG) {
        usleep(random() % 10000);
    }

    pthread_mutex_lock(&db_mutex);
    prerequest_process(request, &idx, &status, &flag);

    pthread_mutex_unlock(&db_mutex);
    fprintf(stderr, "idx: %d, status: %c, flag: %c\n", idx, status, flag);

    if (status == OP_INVALID || status == OP_BLOCKED ||
        status == OP_EXCEED_STORE) {
        command_reply_t* reply = malloc(sizeof(*reply));
        reply->status = 'X';
        send_reply(sock_fd, reply, flag);
        free(reply);
        return 1;
    }
    if (idx == -1) {
        fprintf(stderr, "OP is valid but idx is %d; status: %c, flag: %c\n",
                idx, status, flag);
        exit(1);
    }
    command_reply_t* reply = malloc(sizeof(*reply));
    init_cmd_reply(reply);

    switch (request->op) {
        case READ_FLAG:
            handle_read(sock_fd, request, reply, idx);
            break;
        case WRITE_FLAG:
            handle_write(sock_fd, request, reply, idx, flag);
            break;
        case OVERWRITE_FLAG:
            handle_write(sock_fd, request, reply, idx, flag);
            break;
        case DELETE_FLAG:
            handle_delete(sock_fd, request, reply, idx);
            break;
        default:
            fprintf(stderr, "unknown command.\n");
            break;
    }

    pthread_mutex_lock(&db_mutex);
    postrequest_update(flag, status, idx);
    pthread_mutex_unlock(&db_mutex);

    // Distinguish between write and overwrite here when update status.
    update_stats(reply, flag);

    char reply_flag = flag == OVERWRITE_FLAG ? WRITE_FLAG : flag;

    send_reply(sock_fd, reply, reply_flag);
    free(request);
    free(reply->data);
    free(reply);
    return 0;
}
void postrequest_update(char flag, char status, int idx) {
    if (idx != -1) {
        switch (flag) {
            case READ_FLAG:
                read_status[idx]--;
                if (read_status[idx] == KEY_VALID) {
                    write_status[idx] = KEY_VALID;
                }
                break;
            case WRITE_FLAG:
                write_status[idx] = KEY_VALID;
                read_status[idx] = KEY_VALID;
                break;
            case OVERWRITE_FLAG:
                write_status[idx] = KEY_VALID;
                read_status[idx] = KEY_VALID;
                break;
            case DELETE_FLAG:
                read_status[idx] = KEY_INVALID;
                write_status[idx] = KEY_INVALID;
            default:
                break;
        }
    }
}

void prerequest_process(command_t* request, int* p_idx, char* p_status,
                        char* p_flag) {
    char flag = request->op;
    *p_flag = flag;
    int idx = search_key(request->name);
    fprintf(stderr, "idx: %d\n", idx);
    if (idx != -1 && flag == WRITE_FLAG) *p_flag = OVERWRITE_FLAG;
    if (idx == -1) {
        if (flag == WRITE_FLAG) {
            idx = search_free_entry();
            if (idx == -1) {
                *p_status = OP_EXCEED_STORE;
            }
        } else {
            *p_status = OP_INVALID;
        }
    }
    *p_idx = idx;
    // All operations need write lock to be available.
    if (write_status[idx] == KEY_BUSY) {
        *p_status = OP_BLOCKED;
    }
    if (idx == -1) return;
    if (flag == READ_FLAG) {
        if (read_status[idx] == KEY_VALID) {
            write_status[idx] = KEY_BUSY;
            read_status[idx] = KEY_BUSY;
        } else {
            if (read_status[idx] == KEY_BUSY) {
                read_status[idx]++;
            }
        }
    } else {
        write_status[idx] = KEY_BUSY;
    }

    fprintf(stderr, "idx: %d, status: %c, flag: %c\n", idx, *p_status, flag);
}

void handle_read(int socket, command_t* request, command_reply_t* reply,
                 int idx) {
    char* store_key = request->name;
    fprintf(stderr, "handle read, read key: %s\n", store_key);

    reply->status = 'X';
    sprintf(reply->len, "%d", 0);
    // read from database

    fprintf(stderr, "DB index: %d\n", idx);
    if (idx >= 0) {
        char path[100];
        create_path(path, idx);
        char buff[MAX];
        int sz = read_data(path, buff);
        reply->data = malloc(sizeof(char) * sz);
        fprintf(stderr, "read data length: %d", sz);
        strncpy(reply->data, buff, sz);
        reply->status = 'K';
        sprintf(reply->len, "%d", sz);
    }
}

void handle_write(int socket, command_t* request, command_reply_t* reply,
                  int idx, char flag) {
    char* store_key = request->name;
    int len = atoi(request->len);

    // read data from socket, for database write.
    char store_val[len];
    read(socket, store_val, len);
    char val_print[len + 1];
    strncpy(val_print, store_val, len);
    val_print[len] = 0;
    fprintf(stderr, "Write request data: %s\n", val_print);

    // add new key to keys array. DB mutex lock is needed here.
    if (flag == WRITE_FLAG) {
        pthread_mutex_lock(&db_mutex);
        char* new_key = malloc(sizeof(char) * len);
        strncpy(new_key, store_key, len);
        keys[idx] = new_key;
        fprintf(stderr, "new key: %s added to key array\n", keys[idx]);
        pthread_mutex_unlock(&db_mutex);
    }

    // write a new entry with non-existing key.
    char path[100];
    create_path(path, idx);
    fprintf(stderr, "write path: %s \n", path);

    if (write_data(path, store_val, len) < 0) {
        reply->status = 'X';
    } else {
        reply->status = 'K';
    }
}

void handle_delete(int socket, command_t* request, command_reply_t* reply,
                   int idx) {
    char* store_key = request->name;

    reply->status = 'X';
    if (index >= 0) {
        // remove database store file.
        char db_filename[100];
        create_path(db_filename, idx);
        if (remove(db_filename) == 0) {
            reply->status = 'K';
            pthread_mutex_lock(&db_mutex);
            free(keys[idx]);
            keys[idx] = NULL;
            fprintf(stderr, "key: %s is removed\n", store_key);
            pthread_mutex_unlock(&db_mutex);

        } else {
            fprintf(stderr, "database file unable to remove.\n");
        }
    }
}

int search_key(char* key) {
    int i;

    for (i = 0; i < MAX_STORE; i++) {
        if (write_status[i] == KEY_VALID && strcmp(keys[i], key) == 0) {
            return i;
        }
    }
    return -1;
}

int search_free_entry() {
    int next_empty = -1;
    int i;
    for (i = 0; i < MAX_STORE; i++) {
        if (read_status[i] == KEY_INVALID && write_status[i] == KEY_INVALID) {
            next_empty = i;
            break;
        }
    }
    return next_empty;
}

void init_cmd(command_t* cmd) {
    if (cmd == NULL) return;
    cmd->data = NULL;
    cmd->op = 'I';  // init op to invalid command.
}

void init_cmd_reply(command_reply_t* reply) {
    if (reply == NULL) return;
    reply->data = NULL;
    reply->status = 'I';  // init status to invalid status.
}

// Reply buffer structure:
// | status (1) |	name (31)|	len (8) |	[data - 'R' only] |
void send_reply(int socket, command_reply_t* reply, char flag) {
    char buffer[MAX];
    bzero(buffer, MAX);
    // copy reply to buffer
    int len = 0;
    buffer[0] = reply->status;

    if (reply->status != 'X' && flag == READ_FLAG) {
        len = atoi(reply->len);
        // read reply needs addtional len and data field. Fills them at
        // index 32, 40 as instructed.
        strncpy(&buffer[32], reply->len, 8);
        // reply data might be NULL here.
        strncpy(&buffer[40], reply->data, len);
        fprintf(stderr, "read reply: len: %d , data: %s\n", len, buffer + 40);
    }

    // only write byte needed by reply.
    write(socket, buffer, len + 40);

    // close connection
    close(socket);
}

void update_stats(command_reply_t* reply, char flag) {
    pthread_mutex_lock(&stat_mutex);
    switch (flag) {
        case READ_FLAG:
            stats->no_read_request++;
            break;
        case WRITE_FLAG:
            stats->no_write_request++;
            break;
        case OVERWRITE_FLAG:
            stats->no_write_request++;
            break;
        case DELETE_FLAG:
            stats->no_delete_request++;
            break;
        default:
            break;
    }
    if (reply->status == 'K') {
        switch (flag) {
            case WRITE_FLAG:
                stats->table_items++;
                break;
            case DELETE_FLAG:
                stats->table_items--;
            default:
                break;
        }
    } else {
        stats->no_failed_request++;
    }
    pthread_mutex_unlock(&stat_mutex);
}

void create_path(char* path, int index) {
    strcpy(path, DB_DIR);
    strcat(path, "/data.");
    char strnum[12];
    sprintf(strnum, "%d", index);
    strcat(path, strnum);
}

// add to the queue
int queue_work(int sock_fd) {
    enqueue(job_queue, sock_fd);
    fprintf(stderr, "Queue work, fd is %d\n", sock_fd);
    return 0;
}

// returns the fd in queue
int get_work() {
    int fd = dequeue(job_queue);
    fprintf(stderr, "Get work, fd is %d\n", fd);
    return fd;
}

int read_data(char* filename, char* buf) {
    int fd = open(filename, O_RDONLY);
    int size = read(fd, buf, MAX);
    printf("size %d\n", size);
    close(fd);
    return size;
}

int write_data(char* filename, char* buf, int len) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd < 0) {
        perror("can't open\n");
        return -1;
    }
    int size = write(fd, buf, len);
    close(fd);
    return size;
}

void display_stats() {
    printf("Number of objects in the table: %d\n", stats->table_items);
    printf("Number of read requests received: %d\n", stats->no_read_request);
    printf("Number of write requests received: %d\n", stats->no_write_request);
    printf("Number of delete requests received: %d\n",
           stats->no_delete_request);
    printf("Number of requests queued waiting for worker threads: %d\n",
           stats->no_request_queued);
    printf("Number of failed requests: %d\n", stats->no_failed_request);
    printf("\n");
}

void free_before_exit() {
    pthread_mutex_lock(&stat_mutex);
    free(stats);
    pthread_mutex_unlock(&stat_mutex);
    pthread_mutex_lock(&queue_cond_mutex);

    free(job_queue);
    pthread_mutex_unlock(&queue_cond_mutex);
    pthread_mutex_lock(&db_mutex);
    int i;
    for (i = 0; i < MAX_STORE; i++) {
        if (keys[i] != NULL) {
            free(keys[i]);
        }
    }
    pthread_mutex_unlock(&db_mutex);
}

void init_global() {
    int i;
    for (i = 0; i < MAX_STORE; i++) {
        keys[i] = NULL;
        read_status[i] = KEY_INVALID;
        write_status[i] = KEY_INVALID;
    }
}

int main(int argc, char** argv) {
    system("rm -f ./tmp/data.*");
    // make directory for database store files.
    mkdir(DB_DIR, 0777);
    init_global();
    char line[128]; /* or whatever */
    stats = (stats_t*)malloc(sizeof(stats_t));
    if (argc > 1) {
        port = atoi(argv[1]);
        printf("Port used: %d\n", port);
    }

    job_queue = create_queue();
    pthread_t tid[5];  // 1 listener, 4 worker
    int worker_ids[5];
    int rc;
    int i = 0;
    rc = pthread_create(&tid[i], NULL, listener, NULL);
    if (rc) {
        perror("Error:unable to create thread\n"), exit(-1);
    }

    sleep(1);

    for (i = 1; i < 5; i++) {
        worker_ids[i] = i;
        rc = pthread_create(&tid[i], NULL, handle_work, (void*)&worker_ids[i]);
        if (rc) {
            printf("Error:unable to create thread, %d\n", rc);
            exit(1);
        }
    }

    while (fgets(line, sizeof(line), stdin) != NULL) {
        // printf("Input: %s\n", line);
        if (strcmp("quit\n", line) == 0) {
            free_before_exit();
            exit(0);
        } else if (strcmp("stats\n", line) == 0) {
            display_stats();
        } else {
            continue;
        }
    }

    pthread_exit(NULL);
}
