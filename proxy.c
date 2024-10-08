#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netdb.h>
#include <regex.h>
#include "csapp.h"
#include "cache.h"


/* Recommended max cache and object sizes */
#define MAX_BUF_SIZE 8192
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef char buf_t[MAX_BUF_SIZE];
typedef struct {
    buf_t host, port, path;
} request_t;

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void* start(void* client_sock);
void serve(int client_fd);
int split(const char* url, request_t* req);
void make(int server_fd, const request_t* req, buf_t request);

int main(int argc, char **argv) {
    int listenfd, *clientfdp;
    
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    listenfd = Open_listenfd(argv[1]);
    init();
    
    while (1) {
        clientlen = sizeof(clientaddr);
        clientfdp = malloc(sizeof(int));
        *clientfdp = Accept(listenfd, (SA*)&clientaddr, &clientlen);
        pthread_create(&tid, NULL, start, clientfdp);
    }

    close(listenfd);
    return 0;
}

void* start(void* client_sock) {
    int client_fd = *((int*)client_sock);
    free(client_sock);
    pthread_detach(pthread_self());

    serve(client_fd);

    close(client_fd);
    return NULL;
}

void serve(int client_fd) {
    rio_t client;
    buf_t buf, method, url, version;
    request_t req;

    rio_readinitb(&client, client_fd);
    if (rio_readlineb(&client, buf, MAX_BUF_SIZE) <= 0) {
        return;
    }

    sscanf(buf, "%s %s %s", method, url, version);
    if (strcasecmp(method, "GET") != 0) {
        return;
    }

    char cached_data[MAX_OBJECT_SIZE];
    int cached_size = load(url, cached_data);
    if (cached_size > 0) {
        rio_writen(client_fd, cached_data, cached_size);
        return;
    }

    if (split(url, &req) < 0) {
        return;
    }
    
    int server_fd = Open_clientfd(req.host, req.port);
    if (server_fd < 0) {
        return;
    }

    make(server_fd, &req, buf);

    rio_t server;
    rio_readinitb(&server, server_fd);
    int n;
    int total_size = 0;
    char cache_buf[MAX_OBJECT_SIZE];

    while ((n = rio_readnb(&server, buf, MAX_BUF_SIZE)) > 0) {
        rio_writen(client_fd, buf, n);
        if (total_size + n <= MAX_OBJECT_SIZE) {
            memcpy(cache_buf + total_size, buf, n);
            total_size += n;
        }
    }

    if (total_size > 0) {
        save(url, cache_buf, total_size);
    }
    
    close(server_fd);
}

int split(const char* url, request_t* req) {
    const char* pattern = "^http://([^:/]+)(:([0-9]+))?(/.*)?$";
    regex_t regex;
    regmatch_t pmatch[5];

    if (regcomp(&regex, pattern, REG_EXTENDED) || regexec(&regex, url, 5, pmatch, 0)) {
        regfree(&regex);
        return -1;
    }
    snprintf(req->host, pmatch[1].rm_eo - pmatch[1].rm_so + 1, "%s", url + pmatch[1].rm_so);
    snprintf(req->port, (pmatch[3].rm_so == -1) ? 3 : pmatch[3].rm_eo - pmatch[3].rm_so + 1, 
             "%s", (pmatch[3].rm_so == -1) ? "80" : url + pmatch[3].rm_so);
    snprintf(req->path, (pmatch[4].rm_so == -1) ? 2 : pmatch[4].rm_eo - pmatch[4].rm_so + 1, 
             "%s", (pmatch[4].rm_so == -1) ? "/" : url + pmatch[4].rm_so);

    regfree(&regex);
    return 0;

}

void make(int server_fd, const request_t* req, buf_t request) {
    buf_t headers;
    sprintf(request, "GET %s HTTP/1.0\r\n", req->path);
    sprintf(headers, "Host: %s\r\n", req->host);
    strcat(headers, user_agent_hdr);
    strcat(headers, "Connection: close\r\n");
    strcat(headers, "Proxy-Connection: close\r\n\r\n");
    strcat(request, headers);
    rio_writen(server_fd, request, strlen(request));
}

