#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "php_spy.h"
#include "SAPI.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

ZEND_EXTENSION();

#ifndef ZTS
zend_spy_globals spy_globals;
#else
int spy_globals_id;
#endif

int spy_connect()
{
    const char* host = "127.0.0.1"; /* localhost */
    const char* port = "42042";
    struct addrinfo *ai_list;
    struct addrinfo *ai_ptr;
    struct addrinfo  ai_hints;
    int status;

    memset(&ai_hints, 0, sizeof(ai_hints));
    ai_hints.ai_flags     = 0;
#ifdef AI_ADDRCONFIG
    ai_hints.ai_flags    |= AI_ADDRCONFIG;
#endif
    ai_hints.ai_family    = AF_UNSPEC;
    ai_hints.ai_socktype  = SOCK_DGRAM;
    ai_hints.ai_addr      = NULL;
    ai_hints.ai_canonname = NULL;
    ai_hints.ai_next      = NULL;

    ai_list = NULL;
    status = getaddrinfo(host, port, &ai_hints, &ai_list);
    if (status != 0) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to resolve host '%s': %s", host, gai_strerror(status));
        return FAILURE;
    }

    int fd = socket(ai_list->ai_family, ai_list->ai_socktype, ai_list->ai_protocol);
    if (fd == -1) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to open socket: %d", fd);
        return FAILURE;
    }

    SPY_G(fd) = fd;
    SPY_G(sockaddr_len) = ai_list->ai_addrlen;
    memcpy(&SPY_G(sockaddr), ai_list->ai_addr, ai_list->ai_addrlen);

    freeaddrinfo(ai_list);
    return SUCCESS;
}

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

    //SPY_G(sockaddr) = NULL;
    SPY_G(sockaddr_len) = -1;
    SPY_G(fd) = -1;

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


    if (SPY_G(fd) >= 0) {
        close(SPY_G(fd));
    }

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
    HashPosition file_idx;
    HashPosition line_idx;

    void **dest = NULL;
    spy_coverage_file *file = NULL;
    spy_coverage_line *line = NULL;

    char *buf;
    if ((buf = emalloc(65536)) == NULL) {
        return;
    }

    // FILE *fp = NULL;
    // fp = fopen("/tmp/spy.log", "w+");
    // if (NULL == fp) {
    //     zend_error(E_ERROR, "Failed to open file");
    //     return;
    // }

    int buf_len = 0;
    buf_len += sprintf(buf + buf_len, "server:%s\n", SPY_G(server_name));
    buf_len += sprintf(buf + buf_len, "script:%s\n", SPY_G(script_name));
    for (zend_hash_internal_pointer_reset_ex(SPY_G(coverage), &file_idx);
         zend_hash_get_current_data_ex(SPY_G(coverage), (void**) &dest, &file_idx) == SUCCESS;
         zend_hash_move_forward_ex(SPY_G(coverage), &file_idx)) {

        file = (spy_coverage_file *) *dest;
        buf_len += sprintf(buf + buf_len, "file:%s\n", file->name);
        for (zend_hash_internal_pointer_reset_ex(file->lines, &line_idx);
            zend_hash_get_current_data_ex(file->lines, (void**) &dest, &line_idx) == SUCCESS;
            zend_hash_move_forward_ex(file->lines, &line_idx)) {

            line = (spy_coverage_line *) *dest;
            buf_len += sprintf(buf + buf_len, "%d:%d ", line->lineno, line->count);
        }
        buf_len += sprintf(buf + buf_len, "\n");
    }
    //fclose(fp);

    spy_connect();
    if (sendto(SPY_G(fd), buf, buf_len, 0, (struct sockaddr *) &SPY_G(sockaddr), SPY_G(sockaddr_len)) == -1) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to send to socket: %d", SPY_G(fd));
        efree(buf);
        return;
    }
    efree(buf);
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
