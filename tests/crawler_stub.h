#ifndef CRAWLER_STUB_H
#define CRAWLER_STUB_H
#include <stddef.h>
/* Only tests: portable, no network. 0=graph, 1=large frontier, 2=too many hrefs. */
extern int crawler_stub_mode;
extern size_t crawler_stub_calls;
#endif
