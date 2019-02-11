#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "db_api_util.h"

static char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'};
static char *decoding_table = NULL;
static int mod_table[] = {0, 2, 1};

static void db_build_decoding_table();

char *db_base64_encode(char *data,
                       size_t input_length,
                       size_t *output_length)
{

    int i =0, j = 0;
    uint32_t temp, a, b, c;

    *output_length = 4 * ((input_length + 2) / 3);

    char *encoded_data = g_malloc0(*output_length);
    if (encoded_data == NULL) return NULL;

    for (i = 0, j = 0; i < input_length;) {

        a = i < input_length ? (unsigned char)data[i++] : 0;
        b = i < input_length ? (unsigned char)data[i++] : 0;
        c = i < input_length ? (unsigned char)data[i++] : 0;

        temp = (a << 0x10) + (b << 0x08) + c;

        encoded_data[j++] = encoding_table[(temp >> 3 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(temp >> 2 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(temp >> 1 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(temp >> 0 * 6) & 0x3F];
    }

    for (i = 0; i < mod_table[input_length % 3]; i++)
        encoded_data[*output_length - 1 - i] = '=';

    return encoded_data;
}


char *db_base64_decode(char *data,
                       size_t input_length,
                       size_t *output_length) 
{

    int i = 0, j = 0;
    uint32_t temp, a, b, c, d;

    if (decoding_table == NULL) db_build_decoding_table();

    if (input_length % 4 != 0) return NULL;

    *output_length = input_length / 4 * 3;
    if (data[input_length - 1] == '=') (*output_length)--;
    if (data[input_length - 2] == '=') (*output_length)--;

    char *decoded_data = g_malloc0(*output_length);
    if (decoded_data == NULL) return NULL;

    for (i = 0, j = 0; i < input_length;) {

        a = data[i] == '=' ? 0 & i++ : decoding_table[(uint8_t)data[i++]];
        b = data[i] == '=' ? 0 & i++ : decoding_table[(uint8_t)data[i++]];
        c = data[i] == '=' ? 0 & i++ : decoding_table[(uint8_t)data[i++]];
        d = data[i] == '=' ? 0 & i++ : decoding_table[(uint8_t)data[i++]];

        temp = (a << 3 * 6)
        + (b << 2 * 6)
        + (c << 1 * 6)
        + (d << 0 * 6);

        if (j < *output_length) decoded_data[j++] = (temp >> 2 * 8) & 0xFF;
        if (j < *output_length) decoded_data[j++] = (temp >> 1 * 8) & 0xFF;
        if (j < *output_length) decoded_data[j++] = (temp >> 0 * 8) & 0xFF;
    }

    return decoded_data;
}


static void db_build_decoding_table() 
{

    if (decoding_table == NULL)
    {
        decoding_table = malloc(256);
        int i = 0;
        
        for (i = 0; i < 64; i++)
            decoding_table[(unsigned char) encoding_table[i]] = i;
    }
    
    return;
}
       

void db_base64_cleanup() {
    free(decoding_table);
}


char *db_cmd_to_str(db_cmd_data_t *cmd)
{
    static char cmd_str[DB_CMD_MAX_LEN + DB_CMD_DATA_MAX_LEN];
    int offset = 0, i = 0;

    if (cmd->cmd_type == DB_CMD_STR1_ONLY)
    {
        return cmd->cmd;
    } 
    else if (cmd->cmd_type == DB_CMD_STR1_STR2)
    {
        snprintf(cmd_str, sizeof(cmd_str), "%s %s", cmd->cmd, cmd->cmd2);
    }
    else if (cmd->cmd_type == DB_CMD_WITH_KEY)
    {
        if (cmd->cmd2[0] != '\0')
            offset += snprintf(cmd_str, sizeof(cmd_str), "%s %s 0x", cmd->cmd, cmd->cmd2);
        else
        {
            if (strncasecmp(cmd->cmd, REDIS_BATCH_END_CMD, strlen(REDIS_BATCH_END_CMD)) == 0)
            {
                offset += snprintf(cmd_str, sizeof(cmd_str), "%s", cmd->cmd);
            }
            else
            {
                offset += snprintf(cmd_str, sizeof(cmd_str), "%s 0x", cmd->cmd);
            }
        }

        if (strncasecmp(cmd->cmd, REDIS_BATCH_END_CMD, strlen(REDIS_BATCH_END_CMD)) != 0)
        {
            for (i=0; i<cmd->key_len; i++)
            {
                offset += snprintf(cmd_str+offset, sizeof(cmd_str)-offset, "%02x", (uint8_t)cmd->key[i]);
            }
        }
    }
    else if (cmd->cmd_type == DB_CMD_WITH_DATA)
    {
        if (cmd->cmd2[0] != '\0')
            offset += snprintf(cmd_str, sizeof(cmd_str), "%s %s 0x", cmd->cmd, cmd->cmd2);
        else
            offset += snprintf(cmd_str, sizeof(cmd_str), "%s 0x", cmd->cmd);

        for (i=0; i<cmd->key_len; i++)
        {
            offset += snprintf(cmd_str+offset, sizeof(cmd_str)-offset, "%02x", (uint8_t)cmd->key[i]);
        }

        offset += snprintf(cmd_str+offset, sizeof(cmd_str)-offset, " %d ", cmd->column_id);

        for (i=0; i<cmd->column_data_len; i++)
        {
            offset += snprintf(cmd_str+offset, sizeof(cmd_str)-offset, "%02x", (uint8_t)cmd->column_data[i]);
        }
        
    }
    else
    {
        snprintf(cmd_str, sizeof(cmd_str), "Unknown cmd type %d", cmd->cmd_type);
    }

    return cmd_str;
}
