#include "php_spy.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

ZEND_DECLARE_MODULE_GLOBALS(spy)

#if COMPILE_DL_SPY
ZEND_GET_MODULE(spy)
#endif

void spy_coverage_line_dtor(void *data)
{
    spy_coverage_line *line = (spy_coverage_line *) data;
    free(line);
}

spy_coverage_file *spy_coverage_file_ctor(const char *filename)
{
    spy_coverage_file *file;

    file = malloc(sizeof(spy_coverage_file));
    file->name = strdup(filename);
    ALLOC_HASHTABLE(file->lines);
    zend_hash_init(file->lines, 128, NULL, spy_coverage_line_dtor, 0);

    return file;
}

void spy_coverage_file_dtor(void *data)
{
    spy_coverage_file *file = (spy_coverage_file *) data;

    zend_hash_destroy(file->lines);
    free(file->name);
    free(file);
}


void php_spy_init_globals(zend_spy_globals *spy_g TSRMLS_DC)
{
    spy_g->code_coverage          = NULL;
    spy_g->previous_filename      = "";
    spy_g->previous_file          = NULL;
    spy_g->previous_mark_filename = "";
    spy_g->previous_mark_file     = NULL;
}

void php_spy_shutdown_globals(zend_spy_globals *spy_g TSRMLS_DC)
{

}

/* execution redirection functions */
zend_op_array* (*old_compile_file)(zend_file_handle* file_handle, int type TSRMLS_DC);
zend_op_array* spy_compile_file(zend_file_handle*, int TSRMLS_DC);

static const zend_function_entry spy_functions[] = {
    PHP_FE(spy_hello, NULL)
    PHP_FE_END
};

zend_module_entry spy_module_entry = {
    STANDARD_MODULE_HEADER,
    "Spy",                  // your extension name
    spy_functions,          // where you define your functions
    PHP_MINIT(spy),         // for module initialization
    PHP_MSHUTDOWN(spy),     // for module shutdown process
    PHP_RINIT(spy),         // for request initialization
    PHP_RSHUTDOWN(spy),     // for reqeust shutdown process
    PHP_MINFO(spy),         // for providing module information
    "0.1",
    STANDARD_MODULE_PROPERTIES
};

PHP_MINIT_FUNCTION(spy) {
    /* Redirect compile and execute functions to our own */
    old_compile_file = zend_compile_file;
    zend_compile_file = spy_compile_file;

    ZEND_INIT_MODULE_GLOBALS(spy, php_spy_init_globals, php_spy_shutdown_globals);

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(spy) {
    /* Reset compile, execute and error callbacks */
    zend_compile_file = old_compile_file;

    return SUCCESS;
}

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(spy)
{
    php_printf("PHP_RINIT_FUNCTION\n");

    ALLOC_HASHTABLE(SPY_G(code_coverage));
    zend_hash_init(SPY_G(code_coverage), 32, NULL, spy_coverage_file_dtor, 0);

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(spy)
{
    php_printf("PHP_RSHUTDOWN_FUNCTION\n");

    zend_hash_destroy(SPY_G(code_coverage));
    FREE_HASHTABLE(SPY_G(code_coverage));
    SPY_G(code_coverage) = NULL;

    return SUCCESS;
}
/* }}} */

PHP_MINFO_FUNCTION(spy) {}

// Your functions here...
PHP_FUNCTION(spy_hello) {
  RETURN_TRUE;
}

/* {{{ zend_op_array srm_compile_file (file_handle, type)
 *    This function provides a hook for the execution of bananas */
zend_op_array *spy_compile_file(zend_file_handle *file_handle, int type TSRMLS_DC)
{
    zend_op_array *op_array;
    op_array = old_compile_file(file_handle, type TSRMLS_CC);

    if (!op_array) {
        return op_array;
    }

    if (op_array->fn_flags & ZEND_ACC_DONE_PASS_TWO) {
        php_printf("Filename: %s\n", op_array->filename);

        unsigned int i;
        for (i = 0; i < op_array->last; i++) {
            zend_op opcode = op_array->opcodes[i];
            spy_count_line(op_array->filename, opcode.lineno TSRMLS_CC);
        }
    }

    return op_array;
}
/* }}} */

inline void spy_count_line(const char *filename, int lineno TSRMLS_DC)
{
    php_printf("Line: %s:%d\n", filename, lineno);

    spy_coverage_file *file;
    spy_coverage_line *line;

    if (zend_hash_exists(SPY_G(code_coverage), filename, sizeof(filename))) {
        php_printf("Key \"%s\" exists\n", filename);
    } else {
        php_printf("Key \"%s\" not exists\n", filename);
    }

    if (zend_hash_find(SPY_G(code_coverage), filename, sizeof(filename), (void **) &file) == FAILURE) {
        php_printf("File %s not exist\n", filename);

        /* The file does not exist, so we add it to the hash */
        //file = spy_coverage_file_ctor(op_array->filename);
        //zend_hash_update(SPY_G(code_coverage), op_array->filename, sizeof(op_array->filename), file, sizeof(spy_coverage_file *), NULL);
    }

    // if (strcmp(SPY_G(previous_filename), filename) == 0) {
    //  file = SPY_G(previous_file);
    // } else {
    //  /* Check if the file already exists in the hash */
    //  //if (!hash_find(SPY_G(code_coverage), filename, strlen(filename), (void *) &file)) {
    //  if (zend_hash_find(SPY_G(code_coverage), filename, sizeof(filename), (void **) &file) == FAILURE) {
    //      php_printf("File %s not exist\n", filename);

    //      /* The file does not exist, so we add it to the hash */
    //      file = spy_coverage_file_ctor(filename);

    //      //hash_add(SPY_G(code_coverage), filename, strlen(filename), file);
    //      // zend_hash_add(SPY_G(code_coverage), filename, strlen(filename) + 1, (void **)&file, sizeof(spy_coverage_file *), NULL);
    //  }
    //  SPY_G(previous_filename) = (char *) filename;
    //  SPY_G(previous_file) = file;
    // }

    // /* Check if the line already exists in the hash */
    // if (!hash_index_find(file->lines, lineno, (void *) &line)) {
    //  line = malloc(sizeof(spy_coverage_line));
    //  line->lineno = lineno;
    //  line->count = 0;

    //  spy_hash_index_add(file->lines, lineno, line);
    // }

    // line->count++;
}
