#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <string.h>

// type definition
typedef struct stats_t {
    unsigned int table_items;
    unsigned int no_read_request;
    unsigned int no_write_request;
    unsigned int no_delete_request;
    unsigned int no_request_queued;
    unsigned int no_failed_request;
}stats_t;

//global variable
stats_t *stats;

// function declaration
int listener(void);
int handle_work(int sock_fd);
int queue_work(int sock_fd);
int get_work();
int read_data(char* filename, char* buf);
int write_data(char* filename, char* buf, int len);

// function definition

int listener() {
    int port = 5000;
    int sock = socket(AF_INET, SOCK_STREAM, 0);         /* (A) */
    struct sockaddr_in addr = {.sin_family = AF_INET,
                                 .sin_port = htons(port),         /* (B) */
                                 .sin_addr.s_addr = 0};
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) /* (C) */
        perror("can't bind"), exit(1);
    if (listen(sock, 2) < 0)               /* (D) */
        perror("listen"), exit(1);

    while (1) {
        int fd = accept(sock, NULL, NULL);    /* (E) */
        printf("file descriptor: %d\n", fd);
    }

    return 0;
}

int read_data(char* filename, char* buf) {
    int fd = open(filename, O_RDONLY);
    int size = read(fd, buf, sizeof(buf));
    printf("size %d\n", size);
    close(fd);
    return size;
}

int write_data(char* filename, char* buf, int len) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd < 0)
        perror("can't open"), exit(0);
    int size = write(fd, buf, len);
    close(fd);
    return size;
}

void display_stats() {
    printf("Number of objects in the table: %d\n", stats->table_items);
    printf("Number of read requests received: %d\n", stats->no_read_request);
    printf("Number of write requests received: %d\n", stats->no_write_request);
    printf("Number of delete requests received: %d\n", stats->no_delete_request);
    printf("Number of requests queued waiting for worker threads: %d\n", stats->no_request_queued);
    printf("Number of failed requests: %d\n", stats->no_failed_request);
    printf("\n");
}

int main() {
    system("rm -f /tmp/data.*");
    char line[128]; /* or whatever */
    stats = (stats_t*) malloc(sizeof(stats_t));
    while (fgets(line, sizeof(line), stdin) != NULL) {
        printf("Input: %s\n", line);
        if(strcmp("quit\n", line) == 0) {
            exit(0);
        }
        else if(strcmp("stats\n", line) == 0) {
            display_stats();
        }
        else {
            continue;
        }
    }
}
