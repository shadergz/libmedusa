#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include <execinfo.h>

#include <time.h>

#include <stdarg.h>

#include <stdlib.h>

#include <string.h>

#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include <medusalog.h>

static void do_message(struct medusa_dispatch_data *dispatch);

medusalog_t* medusa_new(medusaattr_t *user_attr, const char **logfilenames, size_t logcount)
{
    medusalog_t *medusa = (medusalog_t*)calloc(sizeof(medusalog_t), 1);

    assert(medusa);

    medusaattr_t *attr = &medusa->attr;

    if (user_attr != NULL)
    {
        memcpy(attr, user_attr, sizeof(medusaattr_t));
    } 
    else
    {
        attr->program = "Medusa Log System";
        attr->maxfmt = 150;
        attr->maxmsg = 200;
    }

    if (user_attr->dolog == NULL)
    {
        assert(logcount && *logfilenames);
        attr->dolog = user_attr->dolog = do_message;
    }
    
    medusa->newfmt = malloc(attr->maxfmt);

    if (logcount > 0)
    {
        medusa->outfiles = (const FILE **)calloc(sizeof(const FILE*), logcount + 1);
        assert(medusa->outfiles);
    }

    const FILE **out = medusa->outfiles;

    char pathbuffer[40];

    while (logfilenames && *logfilenames != NULL)
    {
        strncpy(pathbuffer, *logfilenames, sizeof(pathbuffer) - 1);
        char *bak;

        char *pathtree;

        /* strtok_r writes and corrut the address, we need to copy the file path to other location */
        pathtree = strtok_r(pathbuffer, "/", &bak);

        for (; pathtree ;)
        {
            char *save = strtok_r(NULL, "/", &bak);
            if (save == NULL)
                /* We're arived at end of the string */
                break;

            struct stat statbuf;

            if (stat(pathtree, &statbuf) != 0)
                if (mkdir(pathtree, S_IRWXU | S_IRGRP | S_IXGRP | S_IWOTH) != 0)
                    exit(fprintf(stderr, "Can't create %s, because: %s\n", save, strerror(errno)));
            
            pathtree = save;
        }

        /* Creating and opening all specified files */
        FILE *ptr = fopen(*logfilenames++, "a");
        assert(ptr != NULL);
        *out++ = ptr;
    }

    /* Initializing the mutex resource */
    pthread_mutex_init(&medusa->release_mutex, NULL);
    
    pthread_mutex_init(&medusa->mutex, NULL);

    pthread_attr_init(&medusa->thread_attr);

    pthread_attr_setdetachstate(
        &medusa->thread_attr, PTHREAD_CREATE_DETACHED
    );

    return medusa;
}

static void medusa_finish(medusalog_t *medusa)
{
    const FILE **out = medusa->outfiles;

    for (; out && *out != NULL; )
        fclose((FILE*)*out++);
    
}

/* When this function returns, every message has been delivery */
static void medusa_wait(medusalog_t *medusa)
{
    bool quit = false;

    while (quit != true)
    {
        pthread_mutex_lock(&medusa->mutex);
        if (medusa->wait == 0)
            quit = true;
        pthread_mutex_unlock(&medusa->mutex);
    }
}

bool medusa_destroy(medusalog_t *medusa)
{
    assert(medusa);

    medusa_wait(medusa);

    medusa_finish(medusa);

    pthread_mutex_lock(&medusa->mutex);
    /* Putting the lock in a invalid state to continue */
    pthread_mutex_lock(&medusa->release_mutex);
    /* Never unlocks the mutex */

    free(medusa->outfiles);

    free(medusa->newfmt);

    pthread_attr_destroy(&medusa->thread_attr);

    pthread_mutex_destroy(&medusa->release_mutex);
    pthread_mutex_destroy(&medusa->mutex);
    
    memset(medusa, 0xff, sizeof(medusalog_t));

    free(medusa);

    return true;
}

struct medusa_thread
{
    medusalog_t *medusa;

    char *message;

    size_t sleep_for;

    pthread_t logthread;

};

static void do_message(struct medusa_dispatch_data *dispatch)
{
    const FILE **out = dispatch->output_files;
    const char *usermessage = dispatch->message;

    for (; *out != NULL; )
        fwrite(usermessage, strlen(usermessage), 1, (FILE*)*out++);    
}

static void* medusa_thread_produce(void *thread_data)
{
    struct medusa_thread *medusa_data = (struct medusa_thread*)thread_data;

    medusalog_t *medusa = medusa_data->medusa;
    medusaattr_t *attr = &medusa->attr;

    struct timespec req = {.tv_sec = medusa_data->sleep_for / 1e+3 /*, .tv_nsec = medusa_data->sleep_for * 1e+6 */};

    /* Going sleep, I need to do this right now either ;) */
    nanosleep(&req, NULL);

    pthread_mutex_lock(&medusa->mutex);
    pthread_mutex_lock(&medusa->release_mutex);

    if (attr->usestdout)
        fprintf(stdout, "%s", medusa_data->message);
    
    struct medusa_dispatch_data *thread_message_data = malloc(sizeof(struct medusa_dispatch_data));
    
    assert(thread_message_data != NULL);

    thread_message_data->output_files = medusa->outfiles;

    thread_message_data->message = medusa_data->message;

    attr->dolog(thread_message_data);

    medusa->wait--;

    free(thread_message_data);

    free(medusa_data->message);

    free(medusa_data);

    pthread_mutex_unlock(&medusa->release_mutex);
    pthread_mutex_unlock(&medusa->mutex);

    return NULL;
}

static int medusa_do(size_t milliseconds, medusa_log_type_t type, medusalog_t *medusa, 
    const char *fmt, va_list va_format)
{
    
    assert(medusa);

    pthread_mutex_lock(&medusa->mutex);

    char *str_type = NULL;

    char str_typebuffer[30];

    switch (type)
    {
    case SUCCESS:   str_type = "Success";   break;
    case INFO:      str_type = "Info";      break;
    case WARNING:   str_type = "Warning";   break;
    case ERROR:     str_type = "Error";     break;
    case DEBUG:     str_type = "Debug";     break;
    default:        str_type = "USER";      break;
    }

    medusaattr_t *attr = &medusa->attr;

    snprintf(str_typebuffer, sizeof(str_typebuffer), "%s%s%s",
        attr->colored ? (
        type == SUCCESS ? "\033[0;32m" :
        type == INFO ? "\033[0;34m" :
        type == WARNING ? "\033[0;33m" :
        type == ERROR ? "\033[0;31m" : 
        type == DEBUG ? "\033[0;35m" : "") : "", 
        str_type, attr->colored ? "\033[0m" : ""
    );

    assert(str_type);

    const char *program = attr->program;

    char date_str[32];

    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);

    rawtime += (milliseconds / 1000);

    timeinfo = localtime(&rawtime);

    strftime(date_str, sizeof(date_str), "%T", timeinfo);

    char auxbuffer[60];

    const size_t maxlen = attr->maxfmt;

    snprintf(auxbuffer, sizeof(auxbuffer), "%s ", program);
    snprintf(medusa->newfmt, maxlen, "%s?",
        attr->printprogram == true ?  auxbuffer : ""
    );

#ifndef __ANDROID__
    /* The way that the backtrace system works on Androd is different */
    void *stack_buffer[4];
    char **stack_strings;

    int symbol_count = backtrace(stack_buffer, sizeof(stack_buffer)/sizeof(void*));

    stack_strings = backtrace_symbols(stack_buffer, symbol_count);

    snprintf(auxbuffer, sizeof(auxbuffer), "\'%s\' - ", strstr(stack_strings[symbol_count - 2], "("));
    snprintf(strstr(medusa->newfmt, "?"), maxlen, "%s?",
        attr->printdebug == true ? auxbuffer : ""
    );

    free(stack_strings);

#endif

    snprintf(auxbuffer, sizeof(auxbuffer), "[%s] ", date_str);
    snprintf(strstr(medusa->newfmt, "?"), maxlen, "%s?",
        attr->printdate == true ? auxbuffer  : ""
    );

    snprintf(auxbuffer, sizeof(auxbuffer), "(%18s): ", str_typebuffer);
    snprintf(strstr(medusa->newfmt, "?"), maxlen, "%s?",
        attr->printtype == true ? auxbuffer : ""
    );

    snprintf(strstr(medusa->newfmt, "?"), maxlen, "%s\n", fmt);

    struct medusa_thread *medusa_data = malloc(sizeof(struct medusa_thread));
    
    assert(medusa_data != NULL);

    medusa_data->medusa = medusa;

    medusa_data->sleep_for = milliseconds;

    medusa_data->message = (char*)malloc(attr->maxmsg);

    assert(medusa_data->message);

    const int ret = vsnprintf(medusa_data->message, attr->maxmsg, medusa->newfmt, va_format);

    medusa->wait++;

    pthread_t thread;

    medusa_data->logthread = pthread_self();

    pthread_mutex_unlock(&medusa->mutex);
    
    if (milliseconds != 0)
        pthread_create(&thread, &medusa->thread_attr, medusa_thread_produce, (void*)medusa_data);
    else medusa_thread_produce(medusa_data);

    return ret;
}

int medusa_log_await(size_t milliseconds, medusa_log_type_t type, medusalog_t *medusa, const char *fmt, ...)
{
    va_list va_format;
    va_start(va_format, fmt);

    int ret = medusa_do(milliseconds, type, medusa, fmt, va_format);

    va_end(va_format);

    return ret;
}

int medusa_log(medusa_log_type_t type, medusalog_t *medusa, const char *fmt, ...)
{
    va_list va_format;
    va_start(va_format, fmt);

    int ret = medusa_do(0, type, medusa, fmt, va_format);

    va_end(va_format);

    return ret;
}

