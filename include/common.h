
#ifndef __COMMON_H__
#define __COMMON_H__

#define PATH_LEN 2048

#ifdef _WIN32
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

const char *getdirname( const char *path );

#endif
