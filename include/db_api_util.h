#ifndef __DATABASE_API_UTIL_H__
#define __DATABASE_API_UTIL_H__

#include "db_api.h"

char *db_base64_encode(char *data,
                       size_t input_length,
                       size_t *output_length);

char *db_base64_decode(char *data,
                       size_t input_length,
                       size_t *output_length) ;

void db_base64_cleanup();

char *db_cmd_to_str(db_cmd_data_t *cmd);

#endif
