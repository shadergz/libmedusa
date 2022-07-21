#include <medusalog.h>

void test_do_message(struct medusa_dispatch_data *medusa_message)
{
    const char *message = medusa_message->message;
    printf("%s", message);
}

int main()
{
    medusaattr_t main_attr = {
        /* Display to standard out (my terminal) */
        .usestdout = false,
        
        /* Display the program name */
        .program = "Medusa Test",

        .printprogram = true,
        
        .printdebug = true,

        .printdate = true,

        .printtype = true,

        /* Display colors :) */
        .colored = true,

        .maxfmt = 150,

        .maxmsg = 200,

        .dolog = test_do_message
    };

    medusalog_t *main_log = medusa_new(&main_attr, NULL, 0);
    
    medusa_log(INFO, main_log, "Main function is located at %p", main);

    medusa_log_await(3000, DEBUG, main_log, "Final message, the log system will be destroyed");

    medusa_log_await(2000, INFO, main_log, "Hmmm this will be printed before the success message"); 

    medusa_log(WARNING, main_log, "I will print a error message, but don't worry, isn't a real error :)");

    medusa_log(ERROR, main_log, "Error message");

    medusa_log(INFO, main_log, "Sleeping for 1 second...");

    medusa_log_await(1000, SUCCESS, main_log, "Everything is ok"); 

    medusa_destroy(main_log);

}

