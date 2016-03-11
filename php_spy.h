#ifndef PHP_SPY_H
#define PHP_SPY_H

#ifdef ZTS
  #include <TSRM.h>
#endif

#include <php.h>
#include <zend_extensions.h>

#ifndef ZEND_EXT_API
# if defined(__GNUC__) && __GNUC__ >= 4
#  define ZEND_EXT_API __attribute__ ((visibility("default")))
# else
#  define ZEND_EXT_API
# endif
#endif

typedef struct spy_coverage_line {
    int lineno;
    int count;
} spy_coverage_line;

void spy_coverage_line_dtor(void *data);

typedef struct spy_coverage_file {
    char       *name;
    HashTable  *lines;
} spy_coverage_file;

spy_coverage_file *spy_coverage_file_ctor(const char *filename);
void spy_coverage_file_dtor(void *data);

typedef struct _spy_globals {
    HashTable            coverage;
    char                 *previous_filename;
    spy_coverage_file    *previous_file;
    int                  counted;
} zend_spy_globals;

#ifdef ZTS
# define SPY_G(v) ZEND_TSRMG(spy_globals_id, zend_spy_globals *, v)
extern int spy_globals_id;
#else
# define SPY_G(v) (spy_globals.v)
extern zend_spy_globals spy_globals;
#endif

#endif /* PHP_SPY_H */
