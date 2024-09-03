#ifndef CACHE_H
#define CACHE_H

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_ENTRIES 10

typedef struct {
    char url[MAXLINE];
    char data[MAX_OBJECT_SIZE];
    int size;
} Entry;

void init();
void save(const char *url, const char *data, int size);
int load(const char *url, char *data);

#endif 
