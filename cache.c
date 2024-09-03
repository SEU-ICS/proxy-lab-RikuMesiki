#include <string.h>
#include "cache.h"

static Entry cache[MAX_ENTRIES];
static int count = 0;

void init() {
    count = 0;
    for (int i = 0; i < MAX_ENTRIES; i++) {
        cache[i].size = 0;
        cache[i].url[0] = '\0';
    }
}

void save(const char *url, const char *data, int size) {
    if (size > MAX_OBJECT_SIZE || size + count > MAX_CACHE_SIZE) {
        return;
    }

    if (count < MAX_ENTRIES) {
        strcpy(cache[count].url, url);
        memcpy(cache[count].data, data, size);
        cache[count].size = size;
        count++;
    }
}

int load(const char *url, char *data) {
    for (int i = 0; i < count; i++) {
        if (strcmp(cache[i].url, url) == 0) {
            memcpy(data, cache[i].data, cache[i].size);
            return cache[i].size;
        }
    }
    return 0;
}
