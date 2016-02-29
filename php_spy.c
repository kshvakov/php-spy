#include "php_spy.h"

ZEND_DECLARE_MODULE_GLOBALS(spy)

#if COMPILE_DL_SPY
ZEND_GET_MODULE(spy)
#endif

void spy_coverage_line_dtor(void *data)
{
	spy_coverage_line *line = (spy_coverage_line *) data;
	free(line);
}

spy_coverage_file *spy_coverage_file_ctor(char *filename)
{
	spy_coverage_file *file;

	file = malloc(sizeof(spy_coverage_file));
	file->name = strdup(filename);
	//file->lines = hash_alloc(128, spy_coverage_line_dtor);
	zend_hash_init(&file->lines, 128, NULL, spy_coverage_line_dtor, 0);

	return file;
}

void spy_coverage_file_dtor(void *data)
{
	spy_coverage_file *file = (spy_coverage_file *) data;

	zend_hash_destroy(&file->lines);
	free(file->name);
	free(file);
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
  "Spy",                       // your extension name
  spy_functions,               // where you define your functions
  PHP_MINIT(spy),     // for module initialization
  PHP_MSHUTDOWN(spy), // for module shutdown process
  PHP_RINIT(spy),      // for request initialization
  PHP_RSHUTDOWN(spy),  // for reqeust shutdown process
  PHP_MINFO(spy),              // for providing module information
  "0.1",
  STANDARD_MODULE_PROPERTIES
};

PHP_MINIT_FUNCTION(spy) {
	/* Redirect compile and execute functions to our own */
	old_compile_file = zend_compile_file;
	zend_compile_file = spy_compile_file;

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
	// XG(code_coverage) = xdebug_hash_alloc(32, xdebug_coverage_file_dtor);
	//zend_hash_init(&SPY_G(code_coverage), 32, NULL, spy_coverage_file_dtor, 0);
	//zend_hash_init(&SPY_G(tags), 10, NULL, php_tag_hash_dtor, 0);
	php_printf("PHP_RINIT_FUNCTION\n");

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(spy)
{
	php_printf("PHP_RSHUTDOWN_FUNCTION\n");
	//zend_hash_destroy(&SPY_G(code_coverage));
	//zend_hash_destroy(&SPY_G(tags));

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

	if (op_array) {
		if (op_array->fn_flags & ZEND_ACC_DONE_PASS_TWO) {
			php_printf("Filename: %s\n", op_array->filename);

			//prefill_from_oparray(STR_NAME_VAL(op_array->filename), op_array TSRMLS_CC);
			unsigned int i;
			for (i = 0; i < op_array->last; i++) {
				zend_op opcode = op_array->opcodes[i];
				//prefill_from_opcode(filename, opcode, 0 TSRMLS_CC);
				spy_count_line(op_array->filename, opcode.lineno TSRMLS_CC);
			}
		}
	}

	return op_array;
}
/* }}} */

inline void spy_count_line(const char *filename, int lineno TSRMLS_DC)
{
	php_printf("Line: %s:%d\n", filename, lineno);

	// spy_coverage_file *file;
	// spy_coverage_line *line;

	// if (strcmp(SG(previous_filename), filename) == 0) {
	// 	file = SG(previous_file);
	// } else {
	// 	/* Check if the file already exists in the hash */
	// 	if (!hash_find(SG(code_coverage), filename, strlen(filename), (void *) &file)) {
	// 		/* The file does not exist, so we add it to the hash */
	// 		file = spy_coverage_file_ctor(filename);

	// 		hash_add(SG(code_coverage), filename, strlen(filename), file);
	// 	}
	// 	SG(previous_filename) = file->name;
	// 	SG(previous_file) = file;
	// }

	// /* Check if the line already exists in the hash */
	// if (!hash_index_find(file->lines, lineno, (void *) &line)) {
	// 	line = malloc(sizeof(spy_coverage_line));
	// 	line->lineno = lineno;
	// 	line->count = 0;

	// 	spy_hash_index_add(file->lines, lineno, line);
	// }

	// line->count++;
}
