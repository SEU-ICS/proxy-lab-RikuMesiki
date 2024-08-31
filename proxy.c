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
} link_info_t;

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void* client_handler(void *arg);
void handle_request(rio_t* rio, buf_t url);
int parse_uri(buf_t url, link_info_t* info);
int build_header(rio_t* rio, buf_t headers, buf_t host);

int main(int argc, char **argv) {
    int listenfd, *connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);

    while (1) {
        clientlen = sizeof(clientaddr);
        connfdp = malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA*)&clientaddr, &clientlen);
        pthread_create(&tid, NULL, client_handler, connfdp);
    }

    close(listenfd);
    return 0;
}

void* client_handler(void* arg) {
    int clientfd = *((int*)arg);
    free(arg);
    pthread_detach(pthread_self());

    rio_t rio;
    buf_t buf, method, url, version;

    rio_readinitb(&rio, clientfd);
    if (rio_readlineb(&rio, buf, MAX_BUF_SIZE) <= 0) {
        close(clientfd);
        return NULL;
    }

    sscanf(buf, "%s %s %s", method, url, version);
    if (strcmp(method, "GET") == 0) {
        handle_request(&rio, url);
    }

    close(clientfd);
    return NULL;
}

int parse_uri(buf_t url, link_info_t* info) {
    if (strncmp(url, "http://", 7) != 0) return -1;

    char *host_start = url + 7;
    char *port_start = strchr(host_start, ':');
    char *path_start = strchr(host_start, '/');

    if (!path_start) return -1;

    if (!port_start) {
        strcpy(info->port, "80");
        *path_start = '\0';
        strcpy(info->host, host_start);
        *path_start = '/';
    } else {
        *port_start = '\0';
        strcpy(info->host, host_start);
        *port_start = ':';
        *path_start = '\0';
        strcpy(info->port, port_start + 1);
        *path_start = '/';
    }
    
    strcpy(info->path, path_start);
    return 0;
}

int build_header(rio_t* rio, buf_t headers, buf_t host) {
    buf_t buf;
    int has_host = 0;

    while (rio_readlineb(rio, buf, MAX_BUF_SIZE) > 0) {
        if (!strcmp(buf, "\r\n")) break;
        if (strstr(buf, "Host:")) has_host = 1;
        if (!strstr(buf, "Connection:") && !strstr(buf, "Proxy-Connection:") && !strstr(buf, "User-Agent:")) {
            strcat(headers, buf);
        }
    }

    if (!has_host) {
        sprintf(buf, "Host: %s\r\n", host);
        strcat(headers, buf);
    }
    strcat(headers, "Connection: close\r\nProxy-Connection: close\r\n");
    strcat(headers, user_agent_hdr);
    strcat(headers, "\r\n");

    return 0;
}

void handle_request(rio_t* rio, buf_t url) {
    link_info_t info;
    if (parse_uri(url, &info) < 0) {
        return;
    }

    buf_t headers;
    build_header(rio, headers, info.host);

    int serverfd = Open_clientfd(info.host, info.port);
    if (serverfd < 0) return;

    rio_t server_rio;
    rio_readinitb(&server_rio, serverfd);

    buf_t buf;
    sprintf(buf, "GET %s HTTP/1.0\r\n%s", info.path, headers);
    rio_writen(serverfd, buf, strlen(buf));

    int n;
    int clientfd = rio->rio_fd;

    while ((n = rio_readnb(&server_rio, buf, MAX_BUF_SIZE)) > 0) {
        rio_writen(clientfd, buf, n);
    }

    close(serverfd);
}




