#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <string.h>

// Macros
#define MAX 4096
#define FILE_NAME "./temp.txt"

// type definition
typedef struct stats_t {
    unsigned int table_items;
    unsigned int no_read_request;
    unsigned int no_write_request;
    unsigned int no_delete_request;
    unsigned int no_request_queued;
    unsigned int no_failed_request;
}stats_t;

typedef struct command_reply_t {
    char op;
    char name[31];
    unsigned char len;
    char *data;
}command_reply_t;

//global variable
stats_t *stats;

// function declaration
int listener(void);
int handle_work(int sock_fd);
int queue_work(int sock_fd);
int get_work();
int read_data(char* filename, char* buf);
int write_data(char* filename, char* buf, int len);
void display_stats();

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

        if (fd < 0)
            perror("server acccept failed...\n"), exit(0); 
        else
            perror("server acccept the client...\n"); 

        printf("file descriptor: %d\n", fd);

        usleep(random() % 10000);

        queue_work(fd);

        handle_work(get_work());
    }

    return 0;
}

int handle_work(int sock_fd){
    char buff[MAX]; 
    int n; 
    // infinite loop for chat 
    for (;;) { 
        bzero(buff, MAX); 
  
        // read the message from client and copy it in buffer 
        read_data(FILE_NAME, buff); 
        // print buffer which contains the client contents 
        printf("From client: %s\t To client : ", buff); 
        bzero(buff, MAX); 
        n = 0; 
        // copy server message in the buffer 
        while ((buff[n++] = getchar()) != '\n') 
            ; 
  
        // and send that buffer to client 
        write(FILE_NAME, buff, sizeof(buff)); 
  
        // if msg contains "Exit" then server exit and chat ended. 
        if (strncmp("quit", buff, 4) == 0) { 
            printf("Server Exit...\n"); 
            break; 
        } else if(strcmp("stats", buff) == 0) {
            display_stats();
        } else {
            continue;
        }
    } 
    return 0;
}

// add to the queue
int queue_work(int sock_fd){
    return 0;
}

// returns the fd in queue
int get_work(){
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

    pthread_t tid[5];   // 1 listener, 4 worker
	int rc;
	int i = 0;
	rc = pthread_create(&tid[i], NULL, listener, NULL;
    if (rc) {
	    perror("Error:unable to create thread\n"), exit(-1);
    }
        
    sleep(1);
        
    for(i=1;i<5;i++) {
        rc = pthread_create(&tid[i], NULL, handle_work, (void *)i);
		if (rc) {
		    printf("Error:unable to create thread, %d\n", rc);
		    exit(-1);
		}
    }
        
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

    pthread_exit(NULL);

}
