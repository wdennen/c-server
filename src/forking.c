/* forking.c: Forking HTTP Server */

#include "spidey.h"

#include <errno.h>
#include <signal.h>
#include <string.h>

#include <unistd.h>

/**
 * Fork incoming HTTP requests to handle the concurrently.
 *
 * @param   sfd         Server socket file descriptor.
 * @return  Exit status of server (EXIT_SUCCESS).
 *
 * The parent should accept a request and then fork off and let the child
 * handle the request.
 **/
int forking_server(int sfd) {
    /* Accept and handle HTTP request */

    signal(SIGCHLD, SIG_IGN);


    while (true) {
        /* Accept request */
        Request *request = accept_request(sfd);
        if(!request){
            log("Unable to accept request: %s" , strerror(errno));
            continue;
        }

        // fork stuff
        pid_t pid = fork();
        if(pid == 0){      // child
            debug("Handle child connection");
            handle_request(request);
            free_request(request);
            exit(EXIT_SUCCESS);
        }
        else               // parent
        {
            free_request(request);
        }

    }

    /* Close server socket */
    return EXIT_SUCCESS;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
