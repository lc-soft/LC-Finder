
#ifndef __COMMON_H__
#define __COMMON_H__

#define PATH_LEN 2048

#ifdef _WIN32
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

const char *getdirname( const char *path );

int pathjoin( char *path, const char *path1, const char *path2 );

int wpathjoin( wchar_t *path, const wchar_t *path1, const wchar_t *path2 );

Dict *StrDict_Create( void *(*val_dup)(void*, const void*),
		      void (*val_del)(void*, void*) );

void StrDict_Release( Dict *d );

#endif
