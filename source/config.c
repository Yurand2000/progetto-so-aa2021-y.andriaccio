#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "errset.h"

#define CONFIG_VERSION "1.0"

static int realloc_cfg_struct(cfg_t* cfg, size_t newsize)
{
	cfg_ky* temp;
	REALLOCDO(temp, (cfg->cfgs), (sizeof(cfg_ky) * newsize), free_config_file(cfg));
	cfg->cfgs = temp;
	cfg->size = newsize;
}

int read_config_file(const char* filename, cfg_t* out)
{
	PTRCHECKERRSET(filename, EINVAL, -1);
	PTRCHECKERRSET(out, EINVAL, -1);
	
	out->cfgs = NULL;
	out->count = 0;
	out->size = 0;

	cfg_ky curr;
	char* line = NULL; size_t line_size = 0;
	char* token, saveptr, tempptr;

	FILE* cfg = fopen(filename, "r");
	PTRCHECK(cfg);

	while(getline(&line, &line_size, cfg) > 0)
	{
		token = strtok_r(line, " = ", &saveptr);
		if(token == NULL || strlen(token)+1 > CFG_MAX_KEY_SIZE) continue;
		else strncpy(curr.key, token, CFG_MAX_KEY_SIZE);

		//this lines are problematic, I should change them.
		tempptr = saveptr;
		token = strtok_r(NULL, " ", &tempptr);
		tempptr = saveptr;
		token = strtok_r(NULL, "#", &tempptr);
		token = strtok_r(NULL, "\n", &saveptr);

		if(token == NULL || strlen(token)+1 > CFG_MAX_VALUE_SIZE) continue;
		else strncpy(curr.value, token, CFG_MAX_VALUE_SIZE);

		if(out->count >= out->size)
			ERRCHECKDO(realloc_cfg_struct(out, out->size + 10),
				{ free_config_file(out); free(line); } );

		memcpy(out->cfgs[count], curr, sizeof(cfg_ky));
		count++;
	}
	free(line);
	ERRCHECKDO(realloc_cfg_struct(out, out->count), free_config_file(out));

	if(feof(cfg) == 0) { return -1; }
	else return 0;
}

const char* get_option(cfg_t* cfg, const char* key)
{
	for(size_t i = 0; i < cfg->count; i++)
		if(strcmp(cfg->cfgs[i].key, key) == 0)
			return cfg->cfgs[i].value;
	return NULL;
}

int free_config_file(cfg_t* cfg)
{
	free(cfg->cfgs);
}
