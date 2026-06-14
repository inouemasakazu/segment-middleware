/****************************************************************************************************
 * @file    cli.c
 * @brief   CLI制御用プログラム
 * @details このファイルにはCLI制御機能を提供するモジュールを定義。
 *
 * @author  Masakazu Inoue
 * @date    2026/05/24          新規作成
 ****************************************************************************************************
 * @note
 * cmd.hにて外部公開しているAPIのみでコマンド機能が使用可能。
 * 
 * 利用手順を以下に示す。
 * 1. cli_init関数でCLIコンテキストの初期化を行う。
 * 2. cli_set_write_func関数で書き込み関数を設定する。
 * 3. cli_cmd_register関数でコマンド登録を行う。
 * 4. cli_printf関数+cli_show_prompt関数で起動時メッセージ+プロンプトを表示する(任意表示)。
 * 5. コマンド実行を行う場合はcli_input_char関数にコマンド入力文字を渡す(\r or \nの入力でコマンドを実行)。
 * 6. 必要に応じてcli_printf関数でコマンド実行結果を出力する。
 * 
 * @attention
 * 入力文字はASCIIコードを使用する。使用可能な文字は以下の通り。
 *  CLI_LINE_EDITTOR   : コマンドラインエディタで使用する文字(例: 'a', 'b', 'c', ...)
 *  CLI_ENTER_CHAR     : コマンド実行を示す文字(例: '\n')
 *  CLI_BACKSPACE_CHAR : コマンドラインエディタで使用する文字(例: '\b')
 *  CLI_ESCAPE_CHAR    : ESCシーンスは未対応(例: '\e')
 ****************************************************************************************************/

/****************************************************************************************************
 * Private include
 ****************************************************************************************************/
#include "cli.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

/****************************************************************************************************
 * Private define
 ****************************************************************************************************/
#ifdef CLI_DEBUG
    #define CLI_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            printf("Assertion failed: %s, file %s, line %d\n", #expr, __FILE__, __LINE__); \
            while (1); \
        } \
    } while (0)
#else
	#define CLI_ASSERT(expr) ((void)0U)
#endif /* CLI_DEBUG */

#define CLI_CMD_NULL       NULL

/*** ASCiiコード(制御文字) ***/
#define NUL                 '\0'    /* null文字 */
#define BS                  '\b'    /* 後退 */
#define HT                  '\t'    /* 水平タブ */
#define LF                  '\n'    /* 改行 */
#define CR                  '\r'    /* 復帰 */
#define ESC                 '\e'    /* エスケープ */
#define DEL                 0x7f    /* 削除 */

/*** ASCiiコード(図形文字) ***/
#define SPC                 ' '     /* 空白文字 */

/*** ESCシーケンス(未使用) ***/
#define ESC_SEQ_UP          "\e[A"  /* 上矢印 */
#define ESC_SEQ_DOWN        "\e[B"  /* 下矢印 */
#define ESC_SEQ_RIGHT       "\e[C"  /* 右矢印 */
#define ESC_SEQ_LEFT        "\e[D"  /* 左矢印 */

/****************************************************************************************************
 * Private typedef
 ****************************************************************************************************/
typedef enum
{
    /*** 入力タイプ ***/
    CLI_INPUT_NONE,
    CLI_INPUT_TEXT,
    CLI_INPUT_ENTER,
    CLI_INPUT_BACKSPACE,
    CLI_INPUT_ESCAPE,
    CLI_INPUT_DELETE,
    CLI_INPUT_MAX
} cli_input_type_t;

/****************************************************************************************************
 * Private Variables
 ****************************************************************************************************/

/****************************************************************************************************
 * Private Functions
 ****************************************************************************************************/

static int cli_cmd_find(cli_context_t *ctx, const char *name, bool is_used);

static cli_input_type_t cli_check_input_type(char c);

static int cli_line_editor(cli_context_t *ctx, char c);

static int cli_execute_cmd(cli_context_t *ctx);
static int cli_tokenizer(cli_context_t *ctx);
static int cli_dispatch(cli_context_t *ctx);

/***
 * @brief ClIコンテキストの初期化
 * @param ctx CLIコンテキスト
 * @return 引数異常(-1) / 成功(0)
 */
int cli_init(cli_context_t *ctx, const char *prompt)
{
    int result = -1;

    if (ctx != NULL)
    {
        memset(ctx, '\0', sizeof(cli_context_t));

        /* プロンプト設定 */
        if (prompt == NULL)
        {
            ctx->prompt = ">";
        }
        else
        {
            ctx->prompt = prompt;
        }

        for (size_t i = 0; i < CLI_CMD_SIZE; i++)
        {
            /* コマンドテーブル初期化 */
            ctx->cmd[i].name[0] = '\0';
            ctx->cmd[i].func = CLI_CMD_NULL;
            ctx->cmd[i].is_used = false;
        }

        result = 0;
    }

    return result;
}

/***
 * @brief プロンプト表示
 * @param ctx CLIコンテキスト
 * @return 引数異常(-1) / 成功(0)
 */
int cli_show_prompt(cli_context_t *ctx)
{
    int result = -1;

    if (ctx != NULL)
    {
        cli_printf(ctx, "\r\n%s", ctx->prompt);

        result = 0;
    }

    return result;
}

/***
 * @brief コマンド登録
 * @param ctx CLIコンテキスト
 * @param name コマンド名
 * @param func コマンド関数
 * @return 引数異常(-1) / 成功(0) / 空きなし(1)
 */
int cli_cmd_register(cli_context_t *ctx, const char *name, cli_func_t func)
{
    int result = -1;

    if ((ctx == NULL) || (name == NULL) || (func == NULL)) return result;

    int index = cli_cmd_find(ctx, name, true);

    if (index >= 0)
    {
        /* 同名のコマンドが登録されている場合は、登録失敗 */
        result = 1;
    }
    else
    {
        /* 空きを探す */
        index = cli_cmd_find(ctx, name, false);
        if (index >= 0)
        {
            /* コマンド登録 */
            snprintf(&ctx->cmd[index].name[0], sizeof(ctx->cmd[index].name), "%s", name);
            ctx->cmd[index].func    = func;
            ctx->cmd[index].is_used = true;
    
            result = 0;     /* 登録成功 */
        }
        else
        {
            result = 1;     /* 空きなし */
        }
    }

    return result;
}

/***
 * @brief コマンド削除
 * @param ctx CLIコンテキスト
 * @param name コマンド名
 * @return 引数異常(-1) / 成功(0) / 見つからなかった(1)
 */
int cli_cmd_unregister(cli_context_t *ctx, const char *name)
{
    int result = -1;

    if ((ctx == NULL) || (name == NULL)) return result;
    
    int index = cli_cmd_find(ctx, name, true);

    if (index >= 0)
    {
        /* コマンド削除 */
        memset(&ctx->cmd[index].name[0], '\0', sizeof(ctx->cmd[index].name));
        ctx->cmd[index].func    = CLI_CMD_NULL;
        ctx->cmd[index].is_used = false;

        result = 0;
    }
    else
    {
        result = 1;
    }

    return result;
}

/***
 * @brief cmd検索
 * @param ctx CLIコンテキスト
 * @param name コマンド名
 * @param is_used 登録されているコマンドを探す場合はtrue、空きを探す場合はfalse
 * @return コマンドテーブルのインデックス(0～) / 見つからなかった(-1)
 */
static int cli_cmd_find(cli_context_t *ctx, const char *name, bool is_used)
{
    int index = -1;

    size_t i;
    bool is_find = false;

    /* 同名のコマンドを探す */
    for (i = 0; i < CLI_CMD_SIZE; i++)
    {
        if (is_used == true)
        {
            /* 登録されているコマンドを探す場合は、同名のコマンドが登録されているかを探す */
            if ((ctx->cmd[i].is_used == true) && (strcmp(name, ctx->cmd[i].name) == 0))
            {
                /* コマンドあり */
                is_find = true;
                break;
            }
        }
        else
        {
            /* 空きを探す場合は、同名のコマンドが登録されていないかを探す */
            if ((ctx->cmd[i].is_used == false) && (strcmp(name, ctx->cmd[i].name) != 0))
            {
                /* 空きあり */
                is_find = true;
                break;
            }
        }
    }

    if (is_find == true)
    {
        /* 一致するコマンドあり */
        index = i;
    }

    return index;
}

/***
 * @name  cli_input_char
 * @brief CLI入力処理
 * @param ctx CLIコンテキスト
 * @param c 入力文字
 * @return 引数異常(-1) / 成功(0)
 */
int cli_input_char(cli_context_t *ctx, char c)
{
    cli_input_type_t type;

    if (ctx == NULL) return -1;

    /* 入力タイプを取得 */
    type = cli_check_input_type(c);

    switch (type)
    {
    case CLI_INPUT_TEXT:
    case CLI_INPUT_BACKSPACE:
        /* コマンドラインエディタ */
        cli_line_editor(ctx, c);
        break;

    case CLI_INPUT_ENTER:
        ctx->current_line.buf[ctx->current_line.size] = '\0';   /* バッファ終端にnull文字を挿入 */

        /* コマンド実行 */
        cli_execute_cmd(ctx);

        /* コマンドライン用バッファ初期化 */
        ctx->current_line.buf[0] = '\0';
        ctx->current_line.size = 0;
        break;

    case CLI_INPUT_ESCAPE:
        /* DO NOTHING */
        break;

    default:
        /* DO NOTHING */
        break;
    }

    return 0;
}

/***
 * @name  cli_check_input_type
 * @brief 入力タイプの判定
 * @param c 入力文字
 * @return 入力タイプ
 */
static cli_input_type_t cli_check_input_type(char c)
{
    cli_input_type_t type = CLI_INPUT_NONE;

    if ((SPC <= c) && (c <= 0x7e))
    {
        /* 図形文字 */
        type = CLI_INPUT_TEXT;
    }
    else if ((c == LF) || (c == CR))
    {
        /* 改行 or 復帰 */
        type = CLI_INPUT_ENTER;
    }
    else if (c == BS)
    {
        /* バックスペース */
        type = CLI_INPUT_BACKSPACE;
    }
    else if (c == ESC)
    {
        /* エスケープシーケンスは未実装 */
    }
    else
    {
        /* その他の制御文字等 */
    }

    return type;
}

/***
 * @name  cli_line_edittor
 * @brief CLIラインエディタ
 * @param ctx CLIコンテキスト
 * @param c 入力文字
 * @return 引数異常(-1) / 成功(0)
 */
static int cli_line_editor(cli_context_t *ctx, char c)
{
    CLI_ASSERT(ctx != NULL);

    if ((SPC <= c) && (c <= 0x7e))
    {
        /* 図形文字(空白含む) */
        if (ctx->current_line.size < (CLI_LINE_SIZE - 1))
        {
            ctx->current_line.buf[ctx->current_line.size] = c;
            ctx->current_line.size++;
        }

        cli_printf(ctx, "%c", c);        /* バッファフローしていてもエコーバックは行う */
    }
    else if (c == BS)
    {
        /* バックスペース */
        if (ctx->current_line.size > 0)
        {
            cli_printf(ctx, "%c", BS );
            cli_printf(ctx, "%c", SPC);
            cli_printf(ctx, "%c", BS );

            ctx->current_line.buf[(ctx->current_line.size - 1)] = '\0';
            ctx->current_line.size--;
        }
    }
    else
    {
        /* 図形文字以外の処理はなし */
    }

    return 0;
}

/***
 * @name  cli_execute_cmd
 * @brief コマンド実行
 * @param ctx CLIコンテキスト
 * @return 引数異常(-1) / 成功(0)
 */
static int cli_execute_cmd(cli_context_t *ctx)
{
    CLI_ASSERT(ctx != NULL);

    /* コマンドラインをトークンに分割 */
    if (cli_tokenizer(ctx) > 0)
    {
        /* コマンドのディスパッチ */
        int result = cli_dispatch(ctx);
        if (result == 1)
        {
            /* コマンド未登録 */
            cli_printf(ctx, "\r\nError: '%s' command not found\r\n", ctx->args.argv[0]);
        }
        else if (result == -1)
        {
            /* コマンド実行エラー */
            cli_printf(ctx, "\r\nError: command execution failed\r\n");
        }
        else
        {
            /* コマンド実行成功 */
        }

        /* コマンド実行後はプロンプトを表示 */
        cli_printf(ctx, "\r\n%s", ctx->prompt);
    }
    else
    {
        /* コマンドラインが空の場合はプロンプトを表示 */
        cli_printf(ctx, "\r\n%s", ctx->prompt);
    }

    return 0;
}

/***
 * @name  cli_tokenizer
 * @brief コマンドラインをスペース区切りでトークンに分割
 * @param ctx CLIコンテキスト
 * @return 引数の数 (0以上)
 * @note 引数の最大数はCLI_ARGV_SIZEで定義されている値まで。引数の最大数を超える場合は切り詰める。
 *       引数の最大数に達していない場合は、argvの最後をNULLにする。
 */
static int cli_tokenizer(cli_context_t *ctx)
{
    CLI_ASSERT(ctx != NULL);
    char *token = ctx->current_line.buf;

    ctx->args.argc = 0;
    memset(ctx->args.argv, '\0', sizeof(ctx->args.argv));

    /* コマンドラインをスペース区切りでトークンに分割 */
    while ((*token != '\0') && (ctx->args.argc < CLI_ARGV_SIZE))
    {
        while (*token == ' ') token++;

        ctx->args.argv[ctx->args.argc] = token;
        ctx->args.argc++;

        while ((*token != ' ') && (*token != '\0'))
        {
            token++;
        }

        if (*token == '\0')
        {
            break;
        }

        *token = '\0';
        token++;
    }

    if (ctx->args.argc < CLI_ARGV_SIZE)
    {
        /* 引数の最大数に達していない場合は、argvの最後をNULLにする */
        ctx->args.argv[ctx->args.argc] = NULL;
    }

    return ctx->args.argc;
}

/***
 * @name  cli_dispatch
 * @brief コマンドのディスパッチ
 * @param ctx CLIコンテキスト
 * @return 異常(-1) / コマンドなし(1) / 成功(0)
 * @note コマンドなしは、コマンドテーブルにコマンドが登録されていない場合を示す。
 *       コマンドなしとコマンド実行エラーは異なる値で返す。
 */
static int cli_dispatch(cli_context_t *ctx)
{
    int result = -1;

    CLI_ASSERT(ctx != NULL);

    if (ctx->args.argc <= 0)
    {
        result = 2;   /* トークンなし */
    }
    else
    {
        /* コマンドを探す */
        int index = cli_cmd_find(ctx, ctx->args.argv[0], true);
        if (index >= 0)
        {
            /* コマンドあり */
            if (ctx->cmd[index].func != CLI_CMD_NULL)
            {
                /* コマンド関数がNULLでない場合は、コマンド実行 */
                result = ctx->cmd[index].func(ctx->args.argc, ctx->args.argv);
            }
            else
            {
                result = -1;  /* コマンド関数未登録 */
            }
        }
        else
        {
            result = 1;   /* コマンドなし */
        }
    }

    return result;
}

/***
 * @name   cli_printf
 * @brief  CLI出力関数  (printfと同様のフォーマットで出力)
 * @param  ctx CLIコンテキスト
 * @param  format フォーマット  (printfと同様)
 * @param  ... 可変引数 (printfと同様)
 * @return 出力した文字数 / エラー(-1)
 */
int cli_printf(cli_context_t *ctx, const char * format, ...)
{
    int size;
    va_list arg;

    va_start(arg, format);

    size = vsnprintf(ctx->write_buf, sizeof(ctx->write_buf), format, arg);

    va_end(arg);

    if (size >= (int)sizeof(ctx->write_buf))
    {
        /* 出力バッファサイズを超える場合は切り詰める */
        size = sizeof(ctx->write_buf) - 1;
        ctx->write_buf[size] = '\0';
    }
    else if (size < 0)
    {
        /* フォーマットエラー等で出力サイズが負の値になる場合はエラーとする */
        size = -1;
    }
    else
    {
        /* 書き込み関数呼び出し */
        if (cli_exe_write_callback(ctx, ctx->write_buf, size) != 0)
        {
            /* エラー */
            size = -1;
        }
    }

    return size;
}

/***
 * @brief 書き込み関数設定
 * @param ctx CLIコンテキスト
 * @param write_cb 書き込みコールバック関数
 * @return 引数異常(-1) / 成功(0)
 */
int cli_set_write_callback(cli_context_t *ctx, cli_write_cb_t write_cb)
{
    int result = -1;

    if (ctx != NULL)
    {
        /* 書き込み関数設定 */
        ctx->write_cb = write_cb;

        result = 0;
    }

    return result;
}

/***
 * @brief 書き込み関数呼び出し
 * @param ctx CLIコンテキスト
 * @param data 書き込みデータのメモリ領域
 * @param size 書き込みデータサイズ
 * @return 引数異常(-1) / 書き込み関数未登録(-1) / 成功(0)
 */
int cli_exe_write_callback(cli_context_t *ctx, const char *data, int size)
{
    int result = -1;

    if (ctx->write_cb == NULL)
    {
        /* 書き込み関数未登録 */
        result = -1;
    }
    else
    {
        if ((size == 0) || (data == NULL))
        {
            /* 引数異常 */
            result = -1;
        }
        else
        {
            /* 書き込み関数呼び出し */
            ctx->write_cb(data, (unsigned short)size);

            result = 0;
        }
    }

    return result;
}
