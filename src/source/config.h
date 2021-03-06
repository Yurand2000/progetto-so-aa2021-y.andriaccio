#ifndef GENERIC_CONFIG_FILE
#define GENERIC_CONFIG_FILE

#include <stdlib.h>

#define CFG_MAX_PAIR_SIZE 512
#define CFG_MAX_KEY_SIZE 64
#define CFG_MAX_VALUE_SIZE (CFG_MAX_PAIR_SIZE - CFG_MAX_KEY_SIZE)

typedef struct config_key_value {
	char key[CFG_MAX_KEY_SIZE];
	char value[CFG_MAX_VALUE_SIZE];
} cfg_kv;

typedef struct config_file {
	cfg_kv* cfgs;
	size_t count;
	size_t size;
} cfg_file_t;

/* reads a configuration file extracting all the possible options.
 * returns 0 on success or -1 on error, sets errno.
 * the current file structure is:
 * [key=value] without the brackets
 * empty lines are allowed and are consequently skipped.
 * lines that do not conform the structures make the function return with an error.
 * line comments are also available, [#comment on the line] without the brackets.
 * never use the symbol '=' in a key name otherwise you might get incorrect readings. */
int read_config_file(const char* filename, cfg_file_t* out);

const char* get_option(cfg_file_t const* cfg, const char* key);

int free_config_file(cfg_file_t* cfg);

#endif
