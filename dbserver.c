#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "queue.h"

// Macros
#define MAX 4096
#define MAX_STORE 200
#define FILE_NAME "./temp.txt"
#define READ_FLAG 'R'
#define WRITE_FLAG 'W'
#define DELETE_FLAG 'D'
#define OVERWRITE_FLAG 'O'
#define DB_DIR "./tmp"

#define KEY_IDLE 0
#define KEY_BUSY 1

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

// global variable
stats_t* stats;
queue_t* job_queue;

pthread_mutex_t queue_cond_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;

char* keys[MAX_STORE];

int status[MAX_STORE];

// function declaration
void* listener(void* ptr);
void* handle_work(void* worker_id);
int handle_work_helper(int socket);
int queue_work(int sock_fd);
int get_work();
int read_data(char* filename, char* buf);
int write_data(char* filename, char* buf, int len);
void display_stats();

void handle_read(int socket, command_t* request);
void handle_write(int socket, command_t* request);
void handle_delete(int socket, command_t* request);

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

// function definition
void* listener(void* ptr) {
    int port = 5000;
    int sock = socket(AF_INET, SOCK_STREAM, 0); /* (A) */
    struct sockaddr_in addr = {.sin_family = AF_INET,
                               .sin_port = htons(port), /* (B) */
                               .sin_addr.s_addr = 0};
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) /* (C) */
        perror("can't bind"), exit(1);
    if (listen(sock, 2) < 0) /* (D) */
        perror("listen"), exit(1);

    while (1) {
        int fd = accept(sock, NULL, NULL); /* (E) */

        if (fd < 0)
            perror("server acccept failed...\n"), exit(0);
        else
            perror("server acccept the client...\n");

        printf("file descriptor: %d\n", fd);

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
    char buff[MAX];
    int n;
    fprintf(stderr, "handle work, fd: %d\n", sock_fd);
    // infinite loop for chat

    bzero(buff, MAX);

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

    switch (request->op) {
        case READ_FLAG:
            handle_read(sock_fd, request);
            break;
        case WRITE_FLAG:
            fprintf(stderr, "int write switch");

            handle_write(sock_fd, request);
            break;
        case DELETE_FLAG:
            handle_delete(sock_fd, request);
            break;
        default:
            fprintf(stderr, "unknown command.\n");
            break;
    }
    free(request);

    return 0;
}

void handle_read(int socket, command_t* request) {
    char* store_key = request->name;
    fprintf(stderr, "handle read, read key: %s\n", store_key);
    command_reply_t* reply = malloc(sizeof(*reply));
    init_cmd_reply(reply);
    reply->status = 'X';
    sprintf(reply->len, "%d", 0);
    // read from database

    int idx = search_key(store_key);
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
    send_reply(socket, reply, READ_FLAG);
    update_stats(reply, READ_FLAG);
    free(reply->data);
    free(reply);
}

void handle_write(int socket, command_t* request) {
    char* store_key = request->name;
    int len = atoi(request->len);

    // read data from socket, for database write.
    char store_val[len];
    read(socket, store_val, len);
    char val_print[len + 1];
    strncpy(val_print, store_val, len);
    val_print[len] = 0;
    fprintf(stderr, "Write request data: %s\n", val_print);

    int overwrite = search_key(store_key);
    int next_empty = search_free_entry();

    int index = overwrite;
    // add new key to keys array.
    if (overwrite == -1) {
        index = next_empty;
        char* new_key = malloc(sizeof(char) * len);
        strncpy(new_key, store_key, len);
        keys[index] = new_key;
        fprintf(stderr, "new key: %s added to key array\n", keys[index]);
    }

    // write a new entry with non-existing key.
    char path[100];
    create_path(path, index);
    fprintf(stderr, "write path: %s \n", path);

    command_reply_t* reply = malloc(sizeof(command_reply_t));
    init_cmd_reply(reply);

    if (write_data(path, store_val, len) < 0) {
        reply->status = 'X';
    } else {
        reply->status = 'K';
    }
    send_reply(socket, reply, WRITE_FLAG);
    // Distinguish between write and overwrite here when update status.
    char flag = overwrite != -1 ? OVERWRITE_FLAG : WRITE_FLAG;
    update_stats(reply, flag);
    free(reply);
}

void handle_delete(int socket, command_t* request) {
    char* store_key = request->name;
    int index = search_key(store_key);
    command_reply_t* reply = malloc(sizeof(*reply));
    init_cmd_reply(reply);
    reply->status = 'X';
    if (index >= 0) {
        // remove database store file.
        char db_filename[100];
        create_path(db_filename, index);
        if (remove(db_filename) == 0) {
            reply->status = 'K';
            free(keys[index]);
            keys[index] = NULL;
            fprintf(stderr, "key: %s is removed\n", store_key);
        } else {
            fprintf(stderr, "database file unable to remove.\n");
        }
    }
    send_reply(socket, reply, DELETE_FLAG);
    update_stats(reply, DELETE_FLAG);
    free(reply);
}

int search_key(char* key) {
    int i;
    for (i = 0; i < MAX_STORE; i++) {
        if (keys[i] != NULL && strcmp(keys[i], key) == 0) {
            return i;
        }
    }
    return -1;
}

int search_free_entry() {
    int next_empty = -1;
    int i;
    for (i = 0; i < MAX_STORE; i++) {
        if (keys[i] == NULL && next_empty == -1) {
            next_empty = i;
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
    if (flag == READ_FLAG) {
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
    free(stats);
    free(job_queue);
    int i;
    for (i = 0; i < MAX_STORE; i++) {
        if (keys[i] != NULL) {
            free(keys[i]);
        }
    }
}

int main() {
    system("rm -f ./tmp/data.*");
    // make directory for database store files.
    mkdir(DB_DIR, 0777);
    char line[128]; /* or whatever */
    stats = (stats_t*)malloc(sizeof(stats_t));
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
            exit(-1);
        }
    }

    while (fgets(line, sizeof(line), stdin) != NULL) {
        printf("Input: %s\n", line);
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
