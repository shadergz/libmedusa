#ifndef MEDUSALOG_H
#define MEDUSALOG_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdio.h>

#include <stdbool.h>

#include <pthread.h>

struct medusa_dispatch_data
{
    const FILE **output_files;
    const char *message;
};

typedef struct
{
    const char *program;

    bool usestdout,
        printprogram,
#ifndef __ANDROID__
        printdebug,
#endif
        printdate,
        printtype,
        colored;

    size_t maxfmt;

    size_t maxmsg;

    void (*dolog)(struct medusa_dispatch_data *dispatch);

} medusaattr_t;

typedef struct
{    
    const FILE **outfiles;

    pthread_mutex_t release_mutex, mutex;

    medusaattr_t attr;

    size_t wait;

    /* Reusable buffer */
    char *newfmt;

    pthread_attr_t thread_attr;

} medusalog_t;

/* Logging level mechanism */
typedef enum 
{
    SUCCESS, INFO, WARNING, ERROR, DEBUG
} medusa_log_type_t;

medusalog_t* medusa_new(medusaattr_t *user_attr, const char **logfilenames, size_t logcount);

bool medusa_destroy(medusalog_t *medusa);

int medusa_log_await(size_t milliseconds, medusa_log_type_t type, medusalog_t *medusa, const char *fmt, ...);

int medusa_log(medusa_log_type_t type, medusalog_t *medusa, const char *fmt, ...);

#if defined(__cplusplus)
}
#endif

#endif
