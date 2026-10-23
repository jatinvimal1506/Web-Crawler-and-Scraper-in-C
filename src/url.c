/* URL parsing, relative-link resolution, and canonical normalization belong here.
 * Keep allocation and ownership rules explicit in the public API. */
#include "url.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


// static:- the function  can only be called from within this file. It is not visible to other files in the project.
static char*copy_text(const char*start,size_t length){
    // this helper allocates a new string, copies the text we want. 
    char *result = malloc(length + 1);
    if (!result) {
        return NULL;
    }
    memcpy(result, start, length);
    result[length] = '\0';
    return result;
}

void url_free(ParsedUrl *url) {
    if(url == NULL){
        return;
    }
    free(url->scheme);
    free(url->host);
    free(url->path);

    url->scheme = NULL;
    url->host = NULL;
    url->path = NULL;
}
int url_parse(const char*input, ParsedUrl*result){
    if(input == NULL || result == NULL){
        return -1;
    }
    //The caller supplies an empty,intialized result.
    result->scheme = NULL;
    result->host = NULL;
    result->path = NULL;

    // Find where the scheme ends
    const char*separator = strstr(input, "://");
    if(separator == NULL){
        return -1; // Invalid URL, no scheme found
    }
    size_t scheme_length = (size_t)(separator - input);
    if(!((scheme_length == 4 ) && (strncmp(input, "http", 4) == 0)) && !((scheme_length == 5) && (strncmp(input, "https", 5) == 0))){
        return -1; // Invalid scheme length
    }
    
    //Everything after :// begins with the host.
    const char*host_start = separator + 3;
    //finding the end of host
    const char*host_end = host_start;
    while(*host_end && *host_end != '/'){
        host_end++;
    }
    if(host_end == host_start){
        return -1; // Invalid URL, no host found
    }
    //basic host validation
    for(const char*p = host_start; p < host_end; p++){
        unsigned char c = (unsigned char)*p;
        if(!isalnum(c) && c != '.' && c != '-'){
            return -1; // Invalid character in host
        }
    }
    const char*path_start = host_end;
    // for now we are not conisdering the  advance url  components for now
    if(strchr(path_start,'?') != NULL || strchr(path_start,'#') != NULL || strchr(path_start,' ') != NULL)
    {
        return -1; // Invalid URL, contains query or fragment
    }

    //Allocate seperate strings for the parsed parts.

    result->scheme = copy_text(input, scheme_length);
    result->host = copy_text(host_start, (size_t)(host_end - host_start));
    result->path = (*path_start == '\0')?copy_text("/",1):copy_text(path_start, strlen(path_start));

    if(result->scheme == NULL || result->host == NULL || result->path == NULL){
        url_free(result);
        return -1; // Memory allocation failed
    }
    return 0;//success

}


