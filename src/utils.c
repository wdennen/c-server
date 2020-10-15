/* utils.c: spidey utilities */

#include "spidey.h"

#include <ctype.h>
#include <errno.h>
#include <string.h>

#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

/**
 * Determine mime-type from file extension.
 *
 * @param   path        Path to file.
 * @return  An allocated string containing the mime-type of the specified file.
 *
 * This function first finds the file's extension and then scans the contents
 * of the MimeTypesPath file to determine which mimetype the file has.
 *
 * The MimeTypesPath file (typically /etc/mime.types) consists of rules in the
 * following format:
 *
 *  <MIMETYPE>      <EXT1> <EXT2> ...
 *
 * This function simply checks the file extension version each extension for
 * each mimetype and returns the mimetype on the first match.
 *
 * If no extension exists or no matching mimetype is found, then return
 * DefaultMimeType.
 *
 * This function returns an allocated string that must be free'd.
 **/
char * determine_mimetype(const char *path) {
    char *ext;
    char *mimetype;
    char *token;
    char buffer[BUFSIZ];
    FILE *fs = NULL;
    /* Find file extension */
    ext = strchr(path, '.');
    ext = strtok(ext, ".");

    mimetype = DefaultMimeType;
    /* Open MimeTypesPath file */
    fs = fopen(MimeTypesPath, "r");
    if (!fs)
    {
        debug("Unable to fopen: %s", strerror(errno));
        return NULL;
    }

    /* Scan file for matching file extensions */
    if (ext) 
    {
        while (fgets(buffer, BUFSIZ, fs))
        {   
            if (*buffer == '#') continue; // ignore comments
            mimetype = skip_whitespace(strtok(buffer, WHITESPACE));
            token = skip_whitespace(strtok(NULL, WHITESPACE));
            while (token)
            {
                if (strcmp(ext, token) == 0)
                {
                    log("Mimetype: %s", mimetype);
                    fclose(fs);
                    return strdup(mimetype);
                }
                token = strtok(NULL, WHITESPACE);
            }
        }
    }
    fclose(fs);
    return strdup(mimetype);
}

/* helper function to see if uri starts with realpath */
bool startRoot(const char *root, const char *uri){
    size_t rootLen = strlen(root),
           uriLen  = strlen(uri);
    return uriLen < rootLen ? false : memcmp(root, uri, rootLen) == 0;
}

/**
 * Determine actual filesystem path based on RootPath and URI.
 *
 * @param   uri         Resource path of URI.
 * @return  An allocated string containing the full path of the resource on the
 * local filesystem.
 *
 * This function uses realpath(3) to generate the realpath of the
 * file requested in the URI.
 *
 * As a security check, if the real path does not begin with the RootPath, then
 * return NULL.
 *
 * Otherwise, return a newly allocated string containing the real path.  This
 * string must later be free'd.
 **/
char * determine_request_path(const char *uri) {
    char buffer[PATH_MAX];
    
    char *catted = cat(root, uri);
    char *fullPat = realpath(catted, buffer);
    if(!fullPat)
    {
        free(catted);
        return NULL;
    }
    char *realURI = strdup(fullPat);

    free(catted);

    if(!realURI){
        debug("this path dont exist");
        return NULL;
    }


    if(!startRoot(RootPath, realURI))
        return NULL; 

    return realURI;
}

/**
 * Return static string corresponding to HTTP Status code.
 *
 * @param   status      HTTP Status.
 * @return  Corresponding HTTP Status string (or NULL if not present).
 *
 * http://en.wikipedia.org/wiki/List_of_HTTP_status_codes
 **/
const char * http_status_string(Status status) {
    static char *StatusStrings[] = {
        "200 OK",
        "400 Bad Request",
        "404 Not Found",
        "500 Internal Server Error",
        "418 I'm A Teapot",
    };

    switch (status)
    {
        case HTTP_STATUS_OK: return StatusStrings[0]; 
                             break;
        case HTTP_STATUS_BAD_REQUEST: return StatusStrings[1];
                                      break;
        case HTTP_STATUS_NOT_FOUND: return StatusStrings[2]; 
                                    break;
        case HTTP_STATUS_INTERNAL_SERVER_ERROR: return StatusStrings[3]; 
                                                break;
        default: return NULL;
                 break;
    }

    return NULL;
}

/**
 * Advance string pointer pass all nonwhitespace characters
 *
 * @param   s           String.
 * @return  Point to first whitespace character in s.
 **/
char * skip_nonwhitespace(char *s) {
    while(s && !(isspace(*s)))
        s++;

    return s;
}

/**
 * Advance string pointer pass all whitespace characters
 *
 * @param   s           String.
 * @return  Point to first non-whitespace character in s.
 **/
char * skip_whitespace(char *s) {
    while(s && isspace(*s))
    {
        s++;
    }

    return s;
}

/** 
 * Get rid of all trailing whitespace better than provided
 * chomp function
 **/
char * chomp(char *s)
{
    int index, i;

    i = 0;
    index = -1;

    while(s[i] != '\0')
    {
        if(s[i] != ' ' && s[i] != '\t' && s[i] != '\n')
        {
            index=i;
        }

        i++;
    }

    s[index + 1] = '\0';

    return s;
}

int retHTML(Request *r, char *file)
{

    FILE *fp;
    char buffer[BUFSIZ];
    char *realPath = determine_request_path(file);
    if(!realPath)
        return -1;
    fp = fopen(realPath, "r");
    if (fp == NULL)
    {
        debug("Could not read html file: %s", realPath);
        return -1;
    }

    while(fgets(buffer, BUFSIZ, fp) != NULL)
        fprintf(r->stream, "%s" ,buffer);

    fclose(fp);
    return 0;
}

char *cat(const char *path, const char *entry)
{  
    char *res = malloc(strlen(path) + strlen(entry) + 1);
    if (!res){
        debug("failed to malloc");
        return NULL;
    }

    strcpy(res, path);
    strcat(res, entry);


    return res;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
