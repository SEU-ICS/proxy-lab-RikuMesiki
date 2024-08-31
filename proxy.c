#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void handle_client(int clientfd);
int parse_uri(char *uri, char *hostname, char *path, char *port);

int main(int argc, char **argv) {
    int listenfd, clientfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        clientfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        handle_client(clientfd);
        Close(clientfd);
    }
    return 0;
}

void handle_client(int clientfd) {
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], path[MAXLINE], port[10];
    rio_t rio;

    Rio_readinitb(&rio, clientfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);

    if (strcasecmp(method, "GET") != 0) {
        printf("Only GET method is supported\n");
        return;
    }

    if (parse_uri(uri, hostname, path, port) < 0) {
        printf("Error parsing URI\n");
        return;
    }

    int serverfd = Open_clientfd(hostname, port);
    if (serverfd < 0) {
        printf("Error connecting to server\n");
        return;
    }

    snprintf(buf, sizeof(buf), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path, hostname);
    Rio_writen(serverfd, buf, strlen(buf));

    while (1) {
        ssize_t n = Rio_readn(serverfd, buf, MAXBUF);
        if (n <= 0) break;
        Rio_writen(clientfd, buf, n);
    }

    Close(serverfd);
}

int parse_uri(char *uri, char *hostname, char *path, char *port) {
    char *host_start, *host_end, *path_start;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0) {
        return -1;
    }

    host_start = uri + 7;
    host_end = strpbrk(host_start, " :/\r\n\0");
    len = host_end - host_start;
    strncpy(hostname, host_start, len);
    hostname[len] = '\0';

    if (*host_end == ':') {
        char *port_start = host_end + 1;
        path_start = strchr(port_start, '/');
        len = path_start - port_start;
        strncpy(port, port_start, len);
        port[len] = '\0';
    } else {
        strcpy(port, "80");
        path_start = host_end;
    }

    if (*path_start == '\0') {
        strcpy(path, "/");
    } else {
        strcpy(path, path_start);
    }

    return 0;
}
