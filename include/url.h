#ifndef URL_H
#define URL_H
//These are header guards. They prevent a header's definitions from being processed more than once in the same compilation unit.
typedef struct {
    char *scheme;    // Example: "https"
    char *host;      // Example: "example.com"
    char *port;      // Example: "8080" (optional)
    char *path;      // Example: "/products"
    char *query;     // Example: "id=5" (optional)
    char *fragment;  // Example: "reviews" (optional)
} ParsedUrl;
int url_parse(const char *input, ParsedUrl *result);//Declares our parsing function. It receives an input URL and a pointer to a structure in which it stores the result.

void url_free(ParsedUrl *url);//Declares a function to release memory allocated for the three strings.


//One important rule: before passing a ParsedUrl to url_parse(), we'll initialize it to zero. That makes its pointers NULL until the parser assigns them.

#endif