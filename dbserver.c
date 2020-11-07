#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

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
    char line[128]; /* or whatever */
    stats = (stats_t*) malloc(sizeof(stats_t));
    while (fgets(line, sizeof(line), stdin) != NULL) {
        printf("%s\n", line);
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
