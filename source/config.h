#ifndef GENERIC_CONFIG_FILE
#define GENERIC_CONFIG_FILE

#include <stdlib.h>

#define CFG_MAX_KEY_SIZE 64
#define CFG_MAX_VALUE_SIZE (256 - CFG_MAX_KEY_SIZE)

typedef struct config_key_value {
	char key[CFG_MAX_KEY_SIZE];
	char value[CFG_MAX_VALUE_SIZE];
} cfg_kv;

typedef struct config_file {
	cfg_kv* cfgs;
	size_t count;
	size_t size;
} cfg_t;

/* reads a configuration file extracting all the possible options.
 * returns 0 on success or -1 on error, sets errno.
 * the current file structure is:
 * [key = value] without the brackets
 * empty lines are allowed and consequently skipped.
 * lines that do not conform the structures make the function return and error.
 * line comments are also available, [#comment on the line] without the brackets.
 * keys might have whitespaces (it is preferred to not use them), but values must not.
 * never use the combination ' = ' in a key name otherwise you might get incorrect readings. */
int read_config_file(const char* filename, cfg_t* out);

const char* get_option(cfg_t* cfg, const char* key);

int free_config_file(cfg_t* cfg);

#endif
