/* handler.c: HTTP Request Handlers */

#include "spidey.h"

#include <errno.h>
#include <limits.h>
#include <string.h>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

/* Internal Declarations */
Status handle_browse_request(Request *request);
Status handle_file_request(Request *request);
Status handle_cgi_request(Request *request);
Status handle_error(Request *request, Status status);

/**
 * Handle HTTP Request.
 *
 * @param   r           HTTP Request structure
 * @return  Status of the HTTP request.
 *
 * This parses a request, determines the request path, determines the request
 * type, and then dispatches to the appropriate handler type.
 *
 * On error, handle_error should be used with an appropriate HTTP status code.
 **/
Status  handle_request(Request *r) {
    Status result;

    /* Parse request */
    int c = parse_request(r);
    if (c < 0)
    {
        debug("Failed to parse request");
        return handle_error(r, HTTP_STATUS_BAD_REQUEST);
    }

    /* Determine request path */
    debug("---URI-----: %s", r->uri);
    debug("---QUERY---: %s", r->query);
    if (r->query) r->uri = strtok(r->uri, "?");

    if (strcmp(r->uri, "/favicon.ico") == 0){
        r->path = determine_request_path("/");
    }
    else{
        r->path = determine_request_path(r->uri);
    }

    if(!r->path)
    {
        debug("Couldn't determine path");
        return handle_error(r, HTTP_STATUS_NOT_FOUND);
    }

    debug("HTTP REQUEST PATH: %s", r->path);

    // Dispatch to appropriate request handler type based on file type 
    struct stat stats;
    if (stat(r->path, &stats) < 0 )
    {
        debug("Stat error");
        return handle_error(r, HTTP_STATUS_NOT_FOUND);
    }

    if (access(r->path, F_OK) < 0)
    {
        debug("Cannot access file");
        return handle_error(r, HTTP_STATUS_BAD_REQUEST);
    }
    if (stats.st_mode & S_IFDIR){ // its a directory
        debug("Browse request");
        result = handle_browse_request(r);
    }
    else if (stats.st_mode & S_IFREG){ // regular file
        debug("Regular File");
        if ((stats.st_mode & S_IXOTH) && access(r->path, X_OK) == 0)   // executable
        {
            debug("CGI request");
            result = handle_cgi_request(r);
        }
        else{
            debug("File request");
            result = handle_file_request(r);
        }
    }


    log("HTTP REQUEST STATUS: %s", http_status_string(result));
    if(result != HTTP_STATUS_OK) 
        return handle_error(r, result);

    return result;
}

/**
 * Handle browse request.
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP browse request.
 *
 * This lists the contents of a directory in HTML.
 *
 * If the path cannot be opened or scanned as a directory, then handle error
 * with HTTP_STATUS_NOT_FOUND.
 **/
Status  handle_browse_request(Request *r) {
    struct dirent **entries;
    int n;

    /* Open a directory for reading or scanning */
    char* path;

    n = scandir(r->path, &entries, 0, alphasort);
    if (n < 0)
    {
        debug("scandir failed: %s", strerror(errno));
        return HTTP_STATUS_NOT_FOUND;
    }

    /* Write HTTP Header with OK Status and text/html Content-Type */
    fprintf(r->stream, "HTTP/1.0 200 OK\r\n");
    fprintf(r->stream, "Content-Type: text/html\r\n");
    fprintf(r->stream, "\r\n");

    /* For each entry in directory, emit HTML list item */
    fprintf(r->stream, "<ul>\n");
    
    /* BOOTSTRAP */
    // retHTML(r, "html/pre.html");
    char buffer[BUFSIZ];
    for (int i = 0; i < n; i++)
    {
        if (strcmp(".", entries[i]->d_name) != 0)
        {
            char *link;
            char *sl = "";
            debug("URI: %s", r->uri);
            if(r->uri[strlen(r->uri) - 1] != '/')
            {
                sl = "/";
            }

            fprintf(r->stream, "<li><a href=\"%s%s%s\">%s</a></li>\n", r->uri, sl, entries[i]->d_name, entries[i]->d_name);
        }
        free(entries[i]);
    }
    fprintf(r->stream, "</ul>\n");
    
    /* BOOTSRAP */
    // retHTML(r, "html/post.html");

    free(entries);
    /* Return OK */
    return HTTP_STATUS_OK;
}

/**
 * Handle file request.
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP file request.
 *
 * This opens and streams the contents of the specified file to the socket.
 *
 * If the path cannot be opened for reading, then handle error with
 * HTTP_STATUS_NOT_FOUND.
 **/
Status  handle_file_request(Request *r) {
    FILE *fs;
    char buffer[BUFSIZ];
    char *mimetype = NULL;
    size_t nread;

    /* Open file for reading */

    fs = fopen(r->path, "r");
    if (!fs)
    {
        debug("fopen failed: %s", strerror(errno));
        return HTTP_STATUS_NOT_FOUND;
    }

    /* Determine mimetype */
    mimetype = determine_mimetype(r->path);

    /* Write HTTP Headers with OK status and determined Content-Type */
    fprintf(r->stream, "HTTP/1.0 200 OK\r\n");
    fprintf(r->stream, "Content-Type: %s\r\n", mimetype);
    fprintf(r->stream, "\r\n");

    /* Read from file and write to socket in chunks */
    nread = fread(buffer, 1, BUFSIZ, fs);
    if (nread < 1) goto fail;
    while (nread > 0)
    {
        fwrite(buffer, 1, nread, r->stream);
        nread = fread(buffer, 1, BUFSIZ, fs);
    }

    /* Close file, deallocate mimetype, return OK */
    fclose(fs);
    free(mimetype);
    return HTTP_STATUS_OK;

fail:
    /* Close file, free mimetype, return INTERNAL_SERVER_ERROR */
    fclose(fs);
    free(mimetype);
    return HTTP_STATUS_INTERNAL_SERVER_ERROR;
}

/**
 * Handle CGI request
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP file request.
 *
 * This popens and streams the results of the specified executables to the
 * socket.
 *
 * If the path cannot be popened, then handle error with
 * HTTP_STATUS_INTERNAL_SERVER_ERROR.
 **/
Status  handle_cgi_request(Request *r) {
    FILE *pfs;
    char buffer[BUFSIZ];

    /* Export CGI environment variables from request:
     * http://en.wikipedia.org/wiki/Common_Gateway_Interface */
    if (setenv("DOCUMENT_ROOT", RootPath, 1))
    {
        debug("ERROR: failed to export DOCUMENT_ROOT");
    }
    if (setenv("SERVER_PORT", Port, 1))
    {
        debug("ERROR: failed to export SERVER_ROOT");
    }
    if (r->query)
    {
        if (setenv("QUERY_STRING", r->query, 1))
        {
            debug("ERROR: failed to export QUERY_STRING");
        }
    } else
    {
        setenv("QUERY_STRING", "\0", 1);
    }
    if (setenv("REMOTE_ADDR", r->host, 1))
    {
        debug("ERROR: failed to export REMOTE_ADDR");
    }
    if (setenv("REMOTE_PORT", r->port, 1))
    {
        debug("ERROR: failed to export REMOTE_PORT");
    }
    if (setenv("REQUEST_METHOD", r->method, 1))
    {
        debug("ERROR: failed to export REQUEST_METHOD");
    }
    if (setenv("REQUEST_URI", r->uri, 1))
    {
        debug("ERROR: failed to export REQUEST_URI");
    }
    if (setenv("SCRIPT_FILENAME", r->path, 1))
    {
        debug("ERROR: failed to export SCRIPT_FILENAME");
    }

    /* Export CGI environment variables from request headers */
    for (Header* h = r->headers; h; h=h->next)
    {
        if (streq(h->name, "Accept") )
        {
            if (setenv("HTTP_ACCEPT", h->data, 1))
            {
                debug("ERROR: failed to export HTTP_ACCEPT");
            }
        }
        if (streq(h->name, "Accept-Encoding") )
        {
            if (setenv("HTTP_ACCEPT_ENCODING", h->data, 1))
            {
                debug("ERROR: failed to export HTTP_ACCEPT_ENCODING");
            }
        }
        if (streq(h->name, "Accept-Language") )
        {
            if (setenv("HTTP_ACCEPT_LANGUAGE", h->data, 1))
            {
                debug("ERROR: failed to export HTTP_ACCEPT_LANGUAGE");
            }
        }
        if (streq(h->name, "Connection") )
        {
            if (setenv("HTTP_CONNECTION", h->data, 1))
            {
                debug("ERROR: failed to export HTTP_CONNECTION");
            }
        }
        if (streq(h->name, "Host") )
        {
            if (setenv("HTTP_HOST", h->data, 1))
            {
                debug("ERROR: failed to export HTTP_HOST");
            }
        }
        if (streq(h->name, "User-Agent") )
        {
            if (setenv("HTTP_USER_AGENT", h->data, 1))
            {
                debug("ERROR: failed to export HTTP_USER_AGENT");
            }
        }
    }

    /* POpen CGI Script */
    pfs = popen(r->path, "r");
    /* Copy data from popen to socket */
    size_t nread = fread(buffer, 1, BUFSIZ, pfs);

    while (nread > 0)
    {
        fwrite(buffer, 1, nread, r->stream);
        nread = fread(buffer, 1, BUFSIZ, pfs);
    }
    /* Close popen, return OK */
    pclose(pfs);
    return HTTP_STATUS_OK;
}

/**
 * Handle displaying error page
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP error request.
 *
 * This writes an HTTP status error code and then generates an HTML message to
 * notify the user of the error.
 **/
Status  handle_error(Request *r, Status status) {
    const char *status_string = http_status_string(status);
    /* Write HTTP Header */
    fprintf(r->stream, "HTTP/1.0 %s\r\n", status_string);
    fprintf(r->stream, "Content-Type: text/html\r\n");
    fprintf(r->stream, "\r\n");

    fprintf(r->stream, "<strong>%s</strong>", status_string);
    /* Return specified status */
    return status;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
