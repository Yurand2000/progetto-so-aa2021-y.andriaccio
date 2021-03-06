#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "errset.h"

//static reallocating function
static int realloc_cfg_struct(cfg_file_t* cfg, size_t newsize)
{
	cfg_kv* temp;
	REALLOCDO(temp, (cfg->cfgs), (sizeof(cfg_kv) * newsize),
		free_config_file(cfg));
	cfg->cfgs = temp;
	cfg->size = newsize;
	return 0;
}

int read_config_file(const char* filename, cfg_file_t* out)
{
	PTRCHECKERRSET(filename, EINVAL, -1);
	PTRCHECKERRSET(out, EINVAL, -1);
	
	out->cfgs = NULL;
	out->count = 0;
	out->size = 0;

	cfg_kv curr;
	char* line = NULL; size_t line_size = 0;
	char* token, *token2, *saveptr, *saveptr2;

	FILE* cfg = fopen(filename, "r");
	if (cfg == NULL)
	{
		if (errno == ENOENT) return 0;
		else return -1;
	}

	while(getline(&line, &line_size, cfg) > 0)
	{
		token = strtok_r(line, "=", &saveptr);
		if(token == NULL || strlen(token)+1 > CFG_MAX_KEY_SIZE)
		{
			ERRSETDO(EINVAL, { free_config_file(out); free(line); }, -1);
		}
		else if(*(saveptr-1) == '\n')
		{
			while(*token == ' ' || *token == '\t')
				token++;

			if(*token == '\n' || *token == '#' || *token == '\0')
				continue;
			else
				ERRSETDO(EINVAL, { free_config_file(out); free(line); }, -1);
		}
		else
		{
			token2 = strtok_r(token, "\t ", &saveptr2);
			strncpy(curr.key, token2, CFG_MAX_KEY_SIZE);
		}

		token = strtok_r(NULL, "#\n", &saveptr);
		if(token == NULL || *token == '\n' || strlen(token)+1 > CFG_MAX_KEY_SIZE)
		{
			ERRSETDO(EINVAL, { free_config_file(out); free(line); }, -1);
		}
		else
		{
			token2 = strtok_r(token, "\t ", &saveptr2);
			strncpy(curr.value, token2, CFG_MAX_VALUE_SIZE);
		}

		if(out->count >= out->size)
			ERRCHECKDO(realloc_cfg_struct(out, out->size + 10),
				{ free_config_file(out); free(line); } );

		memcpy( &(out->cfgs[out->count]), &curr, sizeof(cfg_kv) );
		out->count++;
	}
	free(line);
	ERRCHECKDO(realloc_cfg_struct(out, out->count), { free_config_file(out); });

	fclose(cfg);
	return 0;
}

const char* get_option(cfg_file_t const* cfg, const char* key)
{
	for(size_t i = 0; i < cfg->count; i++)
		if(strcmp(cfg->cfgs[i].key, key) == 0)
			return cfg->cfgs[i].value;
	return NULL;
}

int free_config_file(cfg_file_t* cfg)
{
	free(cfg->cfgs);
	return 0;
}
