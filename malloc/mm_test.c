#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

/* Function pointers to hw3 functions */
void* (*mm_malloc)(size_t);
void* (*mm_realloc)(void*, size_t);
void (*mm_free)(void*);

void load_alloc_functions() {
    void *handle = dlopen("hw3lib.so", RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    char* error;
    mm_malloc = dlsym(handle, "mm_malloc");
    if ((error = dlerror()) != NULL)  {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    mm_realloc = dlsym(handle, "mm_realloc");
    if ((error = dlerror()) != NULL)  {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    mm_free = dlsym(handle, "mm_free");
    if ((error = dlerror()) != NULL)  {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }
}

int main() {
    load_alloc_functions();

    int *data = (int*) mm_malloc(sizeof(int));
    assert(data != NULL);
    data[0] = 0x162;
    mm_free(data);
    printf("malloc test successful!\n");

    // char *A = (char *) mm_malloc(256);
    // printf("Address of A is: %p\n", (void *)A);
    // char *B = (char *) mm_malloc(256);
    // printf("Address of B is: %p\n", (void *)B);
    // mm_free(A);
    // char *C = (char *) mm_malloc(512);
    // printf("Address of C is: %p\n", (void *)C);
    // char *c_realloc = (char *) mm_realloc(C, 256);
    // printf("Address of c_realloc is: %p\n", (void *)c_realloc);

    return 0;
}
