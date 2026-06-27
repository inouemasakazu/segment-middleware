/****************************************************************************************************
 * @file    cmd.c
 * @brief   コマンド処理モジュール
 * @details このファイルにはコマンド処理機能を提供するモジュールを定義。
 *
 * @author  Masakazu Inoue
 * @date    2026/05/25     Create New.
 ****************************************************************************************************
 * @note
 * cmd.hにて外部公開しているAPIのみでコマンド機能が使用可能。
 * コマンド実行を行う場合は先に初期化処理を行う。初期化後にコマンド実行は可能となる。
 ****************************************************************************************************/

/****************************************************************************************************
 * Private include
 ****************************************************************************************************/
#include "cmd.h"

#include "CLI/cli.h"
#include "UART/uart.h"

#include "display.h"
#include "../../../../inc/segdisp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/****************************************************************************************************
 * Private define
 ****************************************************************************************************/
#define CMD_BUF_SIZE   32

/****************************************************************************************************
 * Private typedef
 ****************************************************************************************************/
typedef struct
{
    char buf[CMD_BUF_SIZE];
    unsigned short head;
    unsigned short tail;
} cmd_t;

/****************************************************************************************************
 * Private Global Variables
 ****************************************************************************************************/
static cli_context_t cli;

static cmd_t cmd;
static int uart_com;

/****************************************************************************************************
 * Private Prototype Declaration
 ****************************************************************************************************/
static int cmd_dispatch(int argc, char **argv);

static void cmd_tx(const char * data, unsigned short size);
static void cmd_tx_cb(void * ctx);
static void cmd_rx_cb(void * ctx, unsigned char data);

/***
 * @brief 初期化
 */
void cmd_init(void)
{
    /* com port open */
    uart_open(UART_COM_DEBUG, &uart_com, 115200);

    /* 送信・受信コールバック登録 */
    uart_set_tx_callback(&uart_com, cmd_tx_cb);
    uart_set_rx_callback(&uart_com, cmd_rx_cb);

    uart_com = 0;       /* 0=IDLE, 1=TX WAIT, 2=RX COMP */

    memset(cmd.buf, '\0', sizeof(cmd.buf));
    cmd.head = 0;
    cmd.tail = 0;

    /* CLI用データ初期化 */
    cli_init(&cli, "example>");

    /* cli_printf用CB登録 */
    cli_set_write_callback(&cli, cmd_tx);

    /* コマンド登録 */
    cli_cmd_register(&cli, "segment", cmd_dispatch);
    cli_cmd_register(&cli, "seg",     cmd_dispatch);

    /* 起動時メッセージ */
    cli_printf(&cli, "\r\nCommand line started.\r\n");
    cli_show_prompt(&cli);      /* プロンプト表示(>) */
}

/***
 * @brief コマンド処理メイン
 */
void cmd_main(void)
{
    char data[16];
    unsigned short size = 0;

    /* 受信バッファチェック */
    if (cmd.head > cmd.tail)
    {
        size = cmd.head - cmd.tail;
    }
    else if (cmd.head < cmd.tail)
    {
        size = sizeof(cmd.buf) - cmd.tail + cmd.head;
    }
    else
    {
        size = 0;   /* データなし */
    }

    /* コマンドがある場合は処理 */
    for (size_t i = 0; i < size; i++)
    {
        data[0] = cmd.buf[cmd.tail];
        data[1] = '\0';

        /* コマンド入力処理 */
        cli_input_char(&cli, data[0]);

        cmd.tail = (cmd.tail + 1) % sizeof(cmd.buf);
    }
}

/***
 * @brief コマンド振り分け
 */
static int cmd_dispatch(int argc, char **argv)
{
    if ((argc >= 3) && (strcmp(&argv[1][0], "set") == 0))
    {
        segdisp_set_text(&segment, (const uint8_t *)&argv[2][0]);
    }

    else if (strcmp(&argv[1][0], "blink") == 0)
    {
        if ((argc >= 4) && (strcmp(&argv[2][0], "on") == 0))
        {
            uint32_t cycle = (uint32_t)atol(&argv[3][0]);
            if (cycle == 0)
            {
                cycle = 500;  /* デフォルトは500ms */
            }

            segdisp_blink_on(&segment, 0, cycle);
        }
        else if ((argc >= 3) && (strcmp(&argv[2][0], "off") == 0))
        {
            segdisp_blink_off(&segment, 0);
        }
    }
    else if ((argc >= 2) && (strcmp(&argv[1][0], "clr") == 0))
    {
        segdisp_set_pattern(&segment, 0, 0);
    }
    else
    {
        return -1;
    }

    return 0;
}

/***
 * @brief コマンド送信
 * @param data 送信データのメモリ領域
 * @param size 送信データサイズ
 */
static void cmd_tx(const char * data, unsigned short size)
{
    uart_tx_start(&uart_com, (unsigned char *)data, size);

    uart_com = 1;
    while (uart_com != 0);  /* 送信完了までまつ(送信完了前に送信要求しない) */
}


/***
 * @brief 送信コールバック
 */
void cmd_tx_cb(void * ctx)
{
    if (ctx == &uart_com)
    {
        uart_com = 0;     /* アイドル状態に戻す */
    }
    else
    {
        /* DO NOTHING */
    }
}

/***
 * @brief 受信コールバック
 */
void cmd_rx_cb(void * ctx, unsigned char data)
{
    if (ctx == &uart_com)
    {
        uart_com = 2;   /* 受信通知 */

        unsigned short next = (cmd.head + 1) % CMD_BUF_SIZE;

        if (next == cmd.tail)
        {
            /* バッファオーバーフロー */
            /* サンプルではデータ破棄として扱う(キー入力の1byte単位受信なのでfullにならない想定) */
        }
        else
        {
            cmd.buf[cmd.head] = data;
            cmd.head = next;
        }
    }
    else
    {
        /* DO NOTHING */
    }
}
