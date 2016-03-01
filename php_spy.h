#ifndef PHP_SPY_H
#define PHP_SPY_H

#ifdef ZTS
  #include <TSRM.h>
#endif

#include <php.h>

typedef struct spy_coverage_line {
    int lineno;
    int count;
} spy_coverage_line;

typedef struct spy_coverage_file {
    char               *name;
    HashTable        *lines;
} spy_coverage_file;

void spy_coverage_line_dtor(void *data);

spy_coverage_file *spy_coverage_file_ctor(const char *filename);
void spy_coverage_file_dtor(void *data);

void spy_count_line(const char *file, int lineno TSRMLS_DC);

extern zend_module_entry spy_module_entry;

PHP_MINIT_FUNCTION(spy);
PHP_MSHUTDOWN_FUNCTION(spy);
PHP_RINIT_FUNCTION(spy);
PHP_RSHUTDOWN_FUNCTION(spy);
PHP_MINFO_FUNCTION(spy);
PHP_FUNCTION(spy_hello);

ZEND_BEGIN_MODULE_GLOBALS(spy)
    HashTable            *code_coverage;
    char                 *previous_filename;
    spy_coverage_file    *previous_file;
    char                 *previous_mark_filename;
    spy_coverage_file    *previous_mark_file;
ZEND_END_MODULE_GLOBALS(spy)

#ifdef ZTS
    #define SPY_G(v) TSRMG(spy_globals_id, zend_spy_globals *, v)
#else
    #define SPY_G(v) (spy_globals.v)
#endif

#endif /* PHP_SPY_H */
