/* request.c: HTTP Request Functions */

#include "spidey.h"

#include <errno.h>
#include <string.h>

#include <unistd.h>

int parse_request_method(Request *r);
int parse_request_headers(Request *r);

/**
 * Accept request from server socket.
 *
 * @param   sfd         Server socket file descriptor.
 * @return  Newly allocated Request structure.
 *
 * This function does the following:
 *
 *  1. Allocates a request struct initialized to 0.
 *  2. Initializes the headers list in the request struct.
 *  3. Accepts a client connection from the server socket.
 *  4. Looks up the client information and stores it in the request struct.
 *  5. Opens the client socket stream for the request struct.
 *  6. Returns the request struct.
 *
 * The returned request struct must be deallocated using free_request.
 **/
Request * accept_request(int sfd) {
    Request *r;

    /* Allocate request struct (zeroed) */
    r = calloc(1, sizeof(Request));
    if (!r) {
        debug("Unable to allocate request: %s", strerror(errno));
        goto fail;
    }
    /* Accept a client */
    struct sockaddr raddr;
    socklen_t rlen = sizeof(struct sockaddr);
    r->fd = accept(sfd, &raddr, &rlen);
    if(r->fd < 0) {
        debug("Unable to accept: %s", strerror(errno));
        goto fail;
    }

    /* Lookup client information */
    // gives ip addr and port
    int status = getnameinfo(&raddr, rlen, r->host, sizeof(r->host), r->port, sizeof(r->port), NI_NUMERICHOST | NI_NUMERICSERV);
    if(status < 0)
    {
        debug("Unable to getnameinfo %s", gai_strerror(status));
        goto fail;
    }

    /* Open socket stream */
    r->stream = fdopen(r->fd, "w+");
    if(!r->stream){
        debug("Unable to fdopen: %s", strerror(errno));
        goto fail;
    }
    
    log("Accepted request from %s:%s", r->host, r->port);
    return r;

fail:
    /* Deallocate request struct */
    free_request(r);
    return NULL;
}

/**
 * Deallocate request struct.
 *
 * @param   r           Request structure.
 *
 * This function does the following:
 *
 *  1. Closes the request socket stream or file descriptor.
 *  2. Frees all allocated strings in request struct.
 *  3. Frees all of the headers (including any allocated fields).
 *  4. Frees request struct.
 **/

void free_headers(Header *h) {
    if (!h)
        return;
    if(h->name)
        free(h->name);
    if(h->data)
        free(h->data);
    Header *prev = h;
    h = h->next;
    free(prev);
    int i = 2;
    while(h)
    {
        if(h->data)
            free(h->data);
        if(h->name)
            free(h->name);
        prev = h;
        h = h->next;
        free(prev);
        debug("********HEADER FREE %d  *********", i++);
    }
}

void free_request(Request *r) {
    if (!r) {
        return;
    }

    /* Close socket or fd */
    if(r->stream)
        fclose(r->stream);
    /* Free allocated strings */
    if(r->method)
        free(r->method);
    if(r->uri)
        free(r->uri);
    if(r->query)
        free(r->query);
    if(r->path){
        free(r->path);
    }
    /* Free headers */
    if(r->headers)
    {
        free_headers(r->headers);
    }
    /* Free request */
    if(r)
        free(r);
}

/**
 * Parse HTTP Request.
 *
 * @param   r           Request structure.
 * @return  -1 on error and 0 on success.
 *
 * This function first parses the request method, any query, and then the
 * headers, returning 0 on success, and -1 on error.
 **/
int parse_request(Request *r) {
    /* Parse HTTP Request Method */
    if (parse_request_method(r) < 0)
        return -1;
    /* Parse HTTP Requet Headers*/
    if (parse_request_headers(r) < 0)
        return -1;

    return 0;
}

/**
 * Parse HTTP Request Method and URI.
 *
 * @param   r           Request structure.
 * @return  -1 on error and 0 on success.
 *
 * HTTP Requests come in the form
 *
 *  <METHOD> <URI>[QUERY] HTTP/<VERSION>
 *
 * Examples:
 *
 *  GET / HTTP/1.1
 *  GET /cgi.script?q=foo HTTP/1.0
 *
 * This function extracts the method, uri, and query (if it exists).
 **/
int parse_request_method(Request *r) {
    char buffer[BUFSIZ];
    char *method;
    char *uri;
    char *query;

    /* Read line from socket */
    if (!fgets(buffer, BUFSIZ, r->stream)) {
        debug("fgets failed");
        goto fail;
    }

    /* Parse method and uri */
    method = strtok(buffer, WHITESPACE);
    if(!method)
    {
        debug("bad method");
        goto fail;
    }
    method = skip_whitespace(method);
    uri = strtok(NULL, WHITESPACE);
    if(!uri)
    {
        debug("bad method");
        goto fail;
    }
    uri = skip_whitespace(uri);


    /* Parse query from uri */
    query = strchr(uri, '?'); // might need to move the pointer once here..
    if (query) query++;
    //query++;    

    /* Record method, uri, and query in request struct */

    // needa use strdup here m8
    r->method   = strdup(method);
    if (!r->method)
    {
        debug("get method failed");
        goto fail;
    }
    r->uri      = strdup(uri);
    if(!r->uri)
    {
        debug("get uri failed");
        goto fail;
    }
    if(query){
        skip_whitespace(query);
        r->query    = strdup(query);
        if(!r->query)
        {
            debug("get query failed");
            goto fail;
        }
    }
    
    debug("HTTP METHOD: %s", r->method);
    debug("HTTP URI:    %s", r->uri);
    debug("HTTP QUERY:  %s", r->query);

    return 0;

fail:
    debug("failure");
    return -1;
}

/**
 * Parse HTTP Request Headers.
 *
 * @param   r           Request structure.
 * @return  -1 on error and 0 on success.
 *
 * HTTP Headers come in the form:
 *
 *  <NAME>: <DATA>
 *
 * Example:
 *
 *  Host: localhost:8888
 *  User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:29.0) Gecko/20100101 Firefox/29.0
 *  Accept: text/html,application/xhtml+xml
 *  Accept-Language: en-US,en;q=0.5
 *  Accept-Encoding: gzip, deflate
 *  Connection: keep-alive
 *
 * This function parses the stream from the request socket using the following
 * pseudo-code:
 *
 *  while (buffer = read_from_socket() and buffer is not empty):
 *      name, data  = buffer.split(':')
 *      header      = new Header(name, data)
 *      headers.append(header)
 **/
int parse_request_headers(Request *r) {
    Header *head = NULL;
    Header **cur = &head;

    char buffer[BUFSIZ];
    char *name;
    char *data;
    
    int i = 1;
    /* Parse headers from socket */
    
    while (fgets(buffer, BUFSIZ, r->stream) && strlen(buffer) > 2)
    {
        *cur = calloc(1, sizeof(**cur));
        if(!*cur)
        {
            debug("could not calloc header");
            goto fail;
        }
        name = strtok(buffer, ":");
        if(!name)
        {
            debug("bad header");
            free_headers(*cur);
            goto fail;
        }
        name = chomp(name);
        name = skip_whitespace(name);
        data = strtok(NULL,   ":");
        if(!data)
        {
            debug("bad header d");
            free_headers(*cur);
            goto fail;
        }
        data = chomp(data);
        data = skip_whitespace(data);
        debug("Name: %s", name);
        debug("data: %s", data);
        (*cur)->name = strdup(name);
        (*cur)->data = strdup(data);
        cur = &(*cur)->next;
    }
    *cur = NULL; // last node null
    
    r->headers = head;
    for (Header *header = r->headers; header; header = header->next) {
        debug("HTTP HEADER %s = %s", header->name, header->data);
    }
    return 0;

fail:
    debug("failure");
    return -1;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
