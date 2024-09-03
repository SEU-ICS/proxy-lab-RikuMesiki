#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netdb.h>
#include "csapp.h"


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
    buf_t buf, method, uri, version;
    request_t req;

    rio_readinitb(&client, client_fd);
    if (rio_readlineb(&client, buf, MAX_BUF_SIZE) <= 0) {
        return;
    }

    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET") != 0) {
        return;
    }

    if (split(uri, &req) < 0) {
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
    
    while ((n = rio_readnb(&server, buf, MAX_BUF_SIZE)) > 0) {
        rio_writen(client_fd, buf, n);
    }

    close(server_fd);
}

int split(const char* url, request_t* req) {
    const char* prefix = "http://";
    if (strncasecmp(url, prefix, strlen(prefix)) != 0) {
        return -1;
    }

    char* url_copy = strdup(url + strlen(prefix));
    char* port_pos = strchr(url_copy, ':');
    char* path_pos = strchr(url_copy, '/');

    if (port_pos) {
        *port_pos = '\0';
        strcpy(req->host, url_copy);
        *port_pos = ':';
        sscanf(port_pos + 1, "%[^/]", req->port);
    } else {
        strcpy(req->port, "80");
        path_pos = strchr(url_copy, '/');
        *path_pos = '\0';
        strcpy(req->host, url_copy);
    }

    if (path_pos) {
        strcpy(req->path, path_pos);
    } else {
        strcpy(req->path, "/");
    }

    free(url_copy);
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


