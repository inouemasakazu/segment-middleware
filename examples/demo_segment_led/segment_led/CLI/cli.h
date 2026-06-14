/****************************************************************************************************
 * @file    cli.h
 * @brief   CLI制御用インターフェース
 * @details このファイルではCLI制御用IF機能を定義しています。
 *
 * @author  Masakazu Inoue
 * @date    2026/05/24     新規作成
 ****************************************************************************************************/
#ifndef __CLI_H__
#define __CLI_H__

/****************************************************************************************************
 * Public include
 ****************************************************************************************************/
#include <stdbool.h>

/****************************************************************************************************
 * Public define
 ****************************************************************************************************/
#define CLI_LINE_SIZE      128      /* 128byte buf */
#define CLI_ARGV_SIZE        8      /* 8 args */

#define CLI_CMD_NAME_SIZE   32      /* 32byte cmd name */
#define CLI_CMD_SIZE        10      /* 10 command */

/****************************************************************************************************
 * Public typedef
 ****************************************************************************************************/
typedef int (*cli_func_t)(int argc, char **argv);
typedef void (*cli_write_cb_t)(const char *data, unsigned short size);

typedef struct
{
    char name[CLI_CMD_NAME_SIZE];
    cli_func_t func;
    bool is_used;
} cli_cmd_t;

typedef struct
{
    int argc;
    char *argv[CLI_ARGV_SIZE];
} cli_line_args_t;

typedef struct
{
    char buf[CLI_LINE_SIZE];
    unsigned short size;
} cli_line_t;

typedef struct
{
    const char *prompt;

    cli_line_t current_line;
    cli_line_args_t args;

    /* cli cmd entry func */
    cli_cmd_t cmd[CLI_CMD_SIZE];

    /* cli write callback */
    cli_write_cb_t write_cb;
    char write_buf[CLI_LINE_SIZE * 2];    /* 書き込み用バッファ */
} cli_context_t;

/****************************************************************************************************
 * Public Variables
 ****************************************************************************************************/

/****************************************************************************************************
 * Public Functions
 ****************************************************************************************************/
int cli_init(cli_context_t *ctx, const char *prompt);

int cli_show_prompt(cli_context_t *ctx);

int cli_cmd_register(cli_context_t *ctx, const char *name, cli_func_t func);
int cli_cmd_unregister(cli_context_t *ctx, const char *name);

int cli_input_char(cli_context_t *ctx, char c);

int cli_printf(cli_context_t *ctx, const char * format, ...);

int cli_set_write_callback(cli_context_t *ctx, cli_write_cb_t write_cb);
int cli_exe_write_callback(cli_context_t *ctx, const char *data, int size);

#endif  /* __CLI_H__ */