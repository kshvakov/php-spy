#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "php_spy.h"
#include "SAPI.h"

ZEND_EXTENSION();

#ifndef ZTS
zend_spy_globals spy_globals;
#else
int spy_globals_id;
#endif

static void spy_globals_ctor(zend_spy_globals *spy_globals)
{
    memset(spy_globals, 0, sizeof(zend_spy_globals));
}

void spy_coverage_line_dtor(void *data)
{
    spy_coverage_line **line = (spy_coverage_line **) data;
    efree(*line);
}

spy_coverage_file *spy_coverage_file_ctor(const char *filename)
{
    spy_coverage_file *file;

    file = emalloc(sizeof(spy_coverage_file));
    file->name = strdup(filename);

    ALLOC_HASHTABLE(file->lines);
    zend_hash_init(file->lines, 128, NULL, spy_coverage_line_dtor, 0);

    return file;
}

void spy_coverage_file_dtor(void *data)
{
    spy_coverage_file **file = (spy_coverage_file **) data;

    zend_hash_destroy((*file)->lines);
    FREE_HASHTABLE((*file)->lines);
    efree((*file));
}

int spy_startup(zend_extension *extension)
{
    TSRMLS_FETCH();
    CG(compiler_options) |= ZEND_COMPILE_EXTENDED_INFO;

    #ifdef ZTS
        spy_globals_id = ts_allocate_id(&spy_globals_id, sizeof(zend_spy_globals),
            (ts_allocate_ctor) spy_globals_ctor, NULL);
    #else
        spy_globals_ctor(&spy_globals);
    #endif

    return SUCCESS;
}

void spy_activate(void)
{
    SPY_G(previous_filename) = "";
    SPY_G(previous_file) = NULL;

    SPY_G(script_name) = sapi_getenv("SCRIPT_NAME", sizeof("SCRIPT_NAME")-1 TSRMLS_CC);
    SPY_G(server_name) = sapi_getenv("SERVER_NAME", sizeof("SERVER_NAME")-1 TSRMLS_CC);
    if (SPY_G(script_name) == NULL) {
        SPY_G(script_name) = SG(request_info).path_translated;
    }
    if (SPY_G(server_name) == NULL) {
        SPY_G(server_name) = "cli";
    }

    ALLOC_HASHTABLE(SPY_G(coverage));
    zend_hash_init(SPY_G(coverage), 32, NULL, spy_coverage_file_dtor, 0);
}

void spy_deactivate(void)
{
    // for testing
    dump_coverage();

    SPY_G(previous_filename) = "";
    SPY_G(previous_file) = NULL;

    SPY_G(script_name) = NULL;
    SPY_G(server_name) = NULL;

    zend_hash_destroy(SPY_G(coverage));
    FREE_HASHTABLE(SPY_G(coverage));
}

size_t hash_string(const void *key, int len)
{
    const    char   *str  = key;
    register size_t  hash = 5381 + len + 1;

    while (*str++) {
        hash = ((hash << 5) + hash) ^ *str;
    }
    return hash;
}

void statement_handler(zend_op_array *op_array)
{
    char *filename = (char *)zend_get_executed_filename(TSRMLS_C);
    spy_coverage_file *file;
    void **dest;
    if (strcmp(SPY_G(previous_filename), filename) == 0) {
        file = SPY_G(previous_file);
    } else {
        size_t key = hash_string(filename, sizeof(filename));
        if (zend_hash_index_find(SPY_G(coverage), key, (void **) &dest) == FAILURE) {
            // The file does not exist, so we add it to the hash
            file = spy_coverage_file_ctor(filename);
            zend_hash_index_update(SPY_G(coverage), key,
                (void **) &file, sizeof(spy_coverage_file *), (void **) &dest);
        }
        file = (spy_coverage_file *) *dest;

        SPY_G(previous_filename) = filename;
        SPY_G(previous_file) = file;
    }

    int lineno = zend_get_executed_lineno(TSRMLS_C);
    spy_coverage_line *line;
    // Check if the line already exists in the hash
    if (zend_hash_index_find(file->lines, lineno, (void **) &dest) == FAILURE) {
        line = emalloc(sizeof(spy_coverage_line));
        line->lineno = lineno;
        line->count = 1;

        zend_hash_index_update(file->lines, lineno,
            (void **) &line, sizeof(spy_coverage_line *), NULL);
    } else {
        line = (spy_coverage_line *) *dest;
        line->count++;
    }
}

void dump_coverage(void)
{
    fprintf(stderr, "\ndump_coverage %s:%s\n", SPY_G(server_name), SPY_G(script_name));
    HashPosition file_idx;
    HashPosition line_idx;

    void **dest = NULL;
    spy_coverage_file *file = NULL;
    spy_coverage_line *line = NULL;

    for (zend_hash_internal_pointer_reset_ex(SPY_G(coverage), &file_idx);
         zend_hash_get_current_data_ex(SPY_G(coverage), (void**) &dest, &file_idx) == SUCCESS;
         zend_hash_move_forward_ex(SPY_G(coverage), &file_idx)) {

        file = (spy_coverage_file *) *dest;
        for (zend_hash_internal_pointer_reset_ex(file->lines, &line_idx);
            zend_hash_get_current_data_ex(file->lines, (void**) &dest, &line_idx) == SUCCESS;
            zend_hash_move_forward_ex(file->lines, &line_idx)) {

            line = (spy_coverage_line *) *dest;
            fprintf(stderr, "%s:%d - %d\n", file->name, line->lineno, line->count);
        }
    }
    fprintf(stderr, "\ndump_coverage end\n");
}


ZEND_EXT_API zend_extension zend_extension_entry = {
    "PHP Spy",                                  /* name */
    "0.0.1",                                    /* version */
    "Oleg Fedoseev",                            /* author */
    "http://github.com/olegfedoseev/php-spy",   /* URL */
    "Copyright (c) 2016",                       /* copyright */

    spy_startup,                            /* startup */
    NULL,                                   /* shutdown */
    spy_activate,                           /* per-script activation */
    spy_deactivate,                         /* per-script deactivation */
    NULL,                                   /* message handler */
    NULL,                                   /* op_array handler */
    statement_handler,                      /* extended statement handler */
    NULL,                                   /* extended fcall begin handler */
    NULL,                                   /* extended fcall end handler */
    NULL,                                   /* op_array ctor */
    NULL,                                   /* op_array dtor */
    STANDARD_ZEND_EXTENSION_PROPERTIES
};
