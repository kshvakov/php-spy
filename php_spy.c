#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "php_spy.h"

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

static void spy_globals_dtor(zend_spy_globals *spy_globals)
{
    fprintf(stderr, "spy_globals_dtor\n");
    if (spy_globals->coverage.nTableSize) {
        zend_hash_destroy(&spy_globals->coverage);
    }

    // zend_hash_destroy(SPY_G(code_coverage));
    // FREE_HASHTABLE(SPY_G(code_coverage));
    // SPY_G(code_coverage) = NULL;

    fprintf(stderr, "spy_globals_dtor done\n");
}

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

void statement_handler(zend_op_array *op_array)
{
    char *filename;
    int lineno;
    filename = (char *)zend_get_executed_filename(TSRMLS_C);
    lineno = zend_get_executed_lineno(TSRMLS_C);

    spy_coverage_file *file;
    spy_coverage_line *line;

    fprintf(stderr, "%s:%d ", filename, lineno);

    if (strcmp(SPY_G(previous_filename), filename) == 0) {
        file = SPY_G(previous_file);
        fprintf(stderr, "| exists \"%s\" ", file->name);
    } else {
        fprintf(stderr, "| new file");
        fprintf(stderr, "SPY_G(coverage) %d", SPY_G(coverage));
        if (zend_hash_find(&SPY_G(coverage), filename, sizeof(filename), (void **) &file) == FAILURE) {
            /* The file does not exist, so we add it to the hash */
            file = spy_coverage_file_ctor(filename);
            zend_hash_update(&SPY_G(coverage), filename, sizeof(filename), file, sizeof(spy_coverage_file *), NULL);

            fprintf(stderr, " \"%s\" ", file->name);
        }
        SPY_G(previous_file) = file;
        SPY_G(previous_filename) = filename;
    }

    /* Check if the line already exists in the hash */
    if (zend_hash_index_find(file->lines, lineno, (void **) &line) == FAILURE) {
        line = malloc(sizeof(spy_coverage_line));
        line->lineno = lineno;
        line->count = 1;

        zend_hash_index_update(file->lines, lineno, line, sizeof(spy_coverage_line *), NULL);
        fprintf(stderr, "| add line %d ", lineno);
    } else {
        line->count++;
        fprintf(stderr, "| line %d exists ", lineno);
    }

    fprintf(stderr, "| count: %d\n", line->count);
}

void function_call_handler(zend_op_array *op_array)
{
    // char *filename;
    // char *function;
    // int lineno;
    // filename = (char *)zend_get_executed_filename(TSRMLS_C);
    // lineno = zend_get_executed_lineno(TSRMLS_C);
    // function = get_active_function_name(TSRMLS_C);

    //fprintf(stderr, "%s:%d - %s\n", filename, lineno, get_active_function_name(TSRMLS_C));
}

int spy_startup(zend_extension *extension)
{
    fprintf(stderr, "spy_startup\n");
    TSRMLS_FETCH();
    CG(compiler_options) |= ZEND_COMPILE_EXTENDED_INFO;

    #ifdef ZTS
        spy_globals_id = ts_allocate_id(&spy_globals_id, sizeof(zend_spy_globals),
            (ts_allocate_ctor) spy_globals_ctor, (ts_allocate_dtor) spy_globals_dtor);
    #else
        spy_globals_ctor(&spy_globals);
    #endif
    fprintf(stderr, "spy_startup done\n");
    return SUCCESS;
}

void spy_activate(void)
{
    fprintf(stderr, "spy_activate\n");
    SPY_G(counted) = 0;
    SPY_G(previous_filename) = "";
    SPY_G(previous_file) = NULL;

    zend_hash_init(&SPY_G(coverage), 32, NULL, spy_coverage_file_dtor, 1);

    //zend_error(E_NOTICE, "spy_activate");
    fprintf(stderr, "spy_activate done\n");
}

void spy_deactivate(void)
{
    fprintf(stderr, "spy_deactivate\n");

    HashPosition file_idx;
    HashPosition line_idx;
    spy_coverage_file *file = NULL;
    spy_coverage_line *line = NULL;

    for (zend_hash_internal_pointer_reset_ex(&SPY_G(coverage), &file_idx);
         zend_hash_get_current_data_ex(&SPY_G(coverage), (void**) &file, &file_idx) == SUCCESS;
         zend_hash_move_forward_ex(&SPY_G(coverage), &file_idx)) {

        fprintf(stderr, "File: %s\n", file->name);

        // for (zend_hash_internal_pointer_reset_ex(file->lines, &line_idx);
        //     zend_hash_get_current_data_ex(file->lines, (void**) &line, &line_idx) == SUCCESS;
        //     zend_hash_move_forward_ex(file->lines, &line_idx)) {

        //     fprintf(stderr, "Line %d - %d\n", line->lineno, line->count);

        // }
    }

    SPY_G(counted) = 0;
    SPY_G(previous_filename) = "";
    SPY_G(previous_file) = NULL;
    fprintf(stderr, "spy_deactivate done\n");
}

ZEND_EXT_API zend_extension zend_extension_entry = {
    "PHP Spy",                                  /* name */
    "0.0.1",                                    /* version */
    "Oleg Fedoseev",                            /* author */
    "http://github.com/olegfedoseev/php-spy",   /* URL */
    "Copyright (c) 2016",                       /* copyright */
    spy_startup,                                /* startup */
    NULL,                                       /* shutdown */
    spy_activate,                           /* per-script activation */
    spy_deactivate,                         /* per-script deactivation */
    NULL,                                   /* message handler */
    NULL,                                   /* op_array handler */
    statement_handler,                      /* extended statement handler */
    function_call_handler, //NULL,                                   /* extended fcall begin handler */
    NULL,                                   /* extended fcall end handler */
    NULL,                                   /* op_array ctor */
    NULL,                                   /* op_array dtor */
    STANDARD_ZEND_EXTENSION_PROPERTIES
};
