/****************************************************************************************************
 * @file    segment.c
 * @brief   セグメントLED制御モジュール
 * @details このファイルにはセグメントLEDの制御機能を提供するモジュールを定義。
 *
 * @author  Masakazu Inoue
 * @date    2026/05/25          新規作成
 ****************************************************************************************************
 * @note
 * セグメントLEDの制御に必要な関数を定義する。
 * 
 ****************************************************************************************************/

/****************************************************************************************************
 * Private include
 ****************************************************************************************************/
#include "../inc/segment.h"

#include <string.h>
#include <stdbool.h>

/****************************************************************************************************
 * Private define
 ****************************************************************************************************/
/***
 * @brief 表示更新を示すbool値
 */

#define SEG_UPDATE_OFF              false
#define SEG_UPDATE_ON               true

#define SEG_VISIBLE_OFF             false
#define SEG_VISIBLE_ON              true

#define SEG_STATIC_CONTROL          0
#define SEG_DYNAMIC_CONTROL         1

/****************************************************************************************************
 * Private typedef
 ****************************************************************************************************/

/****************************************************************************************************
 * Private Global Variables
 ****************************************************************************************************/

/****************************************************************************************************
 * Private Prototype Declaration
 ****************************************************************************************************/
static void seg_call_draw_cb(seg_ctx_t *ctx, uint8_t length, uint8_t position);

static void seg_enter_dynamic_mode(seg_ctx_t *ctx);
static void seg_enter_static_mode(seg_ctx_t *ctx);

/***
 * @name   seg_init
 * @brief  セグメント状態の初期化
 * @param  ctx セグメント状態のメモリ領域
 * @param  length 使用する桁長
 * @return 処理結果
 */
int seg_init(seg_ctx_t *ctx, uint8_t length)
{
    if (ctx == NULL) return SEG_ARG_ERROR;
    if ((length == 0) || (SEG_DIGIT_LENGTH < length)) return SEG_ARG_ERROR;

    for (size_t i = 0; i < SEG_DIGIT_LENGTH; i++)
    {
        ctx->digit[i].pattern = 0x00000000;             /* bitパターンは0クリア */
        ctx->digit[i].state   = SEG_STATE_COMMON;       /* 通常表示 */
        ctx->digit[i].blink_cycle = 1000;               /* 1000ms cycle */
    }

    ctx->len = length;
    ctx->pos = 0;

    ctx->control = SEG_STATIC_CONTROL;      /* 初期状態はstatic制御 */

    ctx->timer      = 0;
    ctx->scan_cycle = SEG_SCAN_CYCLE_DEFAULT_MS;
    ctx->last_scan  = 0;

    ctx->draw_cb = NULL;
    ctx->encode_cb = NULL;

    return SEG_OK;
}

/***
 * @name   seg_set_draw_cb
 * @brief  描画コールバック登録
 * @param  ctx セグメント状態のメモリ領域
 * @param  cb  コールバック関数のメモリ領域
 * @return 処理結果
 */
int seg_set_draw_cb(seg_ctx_t *ctx, segment_draw_cb_t cb)
{
    if (ctx == NULL) return SEG_ARG_ERROR;

    ctx->draw_cb = cb;

    return SEG_OK;
}

/***
 * @name   seg_set_encode_cb
 * @brief  エンコードコールバック登録
 * @param  ctx セグメント状態のメモリ領域
 * @param  cb  コールバック関数のメモリ領域
 * @return 処理結果
 */
int seg_set_encode_cb(seg_ctx_t *ctx, segment_encode_cb_t cb)
{
    if (ctx == NULL) return SEG_ARG_ERROR;

    ctx->encode_cb = cb;

    return SEG_OK;
}

/***
 * @brief  更新処理
 * @param  ctx セグメント状態のメモリ領域
 * @param  period 内部タイマーの更新周期
 * @return 処理結果
 */
int seg_update(seg_ctx_t *ctx, uint32_t period)
{
    uint8_t len = 0;
    uint8_t pos = 0;
    bool update = SEG_UPDATE_ON;

    if (ctx == NULL) return SEG_ARG_ERROR;

    ctx->timer += period;

    if (ctx->control == SEG_DYNAMIC_CONTROL)
    {
        /* ダイナミック制御は描画更新のため周期判定を行う */
        if ((ctx->timer - ctx->last_scan) >= ctx->scan_cycle)
        {
            ctx->last_scan += ctx->scan_cycle;

            len = 1;
            pos = ctx->pos;
            ctx->pos = (ctx->pos + 1) % ctx->len;   /* scan位置の更新 */
        }
        else
        {
            /* 周期未到達のため更新なし */
            update = SEG_UPDATE_OFF;
        }
    }
    else
    {
        /* スタティック制御は条件なし、無条件更新 */
        len = ctx->len;
    }

    if (update == SEG_UPDATE_ON)
    {
        /* 描画更新 */
        seg_call_draw_cb(ctx, len, pos);
    }

    return SEG_OK;
}

/***
 * @name  seg_call_draw_cb
 * @brief 描画コールバック呼び出し
 *        描画データの生成、及びコールバック処理の起動
 * @param ctx セグメント状態のメモリ領域
 * @param length 描画データ長
 * @param position 描画データ位置
 */
static void seg_call_draw_cb(seg_ctx_t *ctx, uint8_t length, uint8_t position)
{
    uint32_t buf[SEG_DIGIT_LENGTH];

    bool is_visible;

    for (size_t i = 0; i < length; i++)
    {
        switch (ctx->digit[(position + i)].state)
        {
        case SEG_STATE_HIDDEN:
            /* 非表示設定 */
            is_visible = SEG_VISIBLE_OFF;
            break;

        case SEG_STATE_BLINK:
            /* 点滅設定 */
            is_visible = (((ctx->timer / ctx->digit[(position + i)].blink_cycle) % 2) == 0);
            break;

        case SEG_STATE_COMMON:
        default:
            /* 表示状態の指定なし */
            is_visible = SEG_VISIBLE_ON;
            break;
        }

        buf[i] = is_visible ? ctx->digit[(position + i)].pattern : 0;
    }

    if (ctx->draw_cb != NULL)
    {
        /* 描画コールバック呼び出し */
        ctx->draw_cb(&buf[0], length, position);
    }
}

/***
 * @brief  テキスト設定
 * @param ctx セグメント状態のメモリ領域
 * @param text 設定するテキストのメモリ領域
 * @return 処理結果
 */
int seg_set_text(seg_ctx_t *ctx, const uint8_t *text)
{
    if ((ctx == NULL) || (*text == '\0')) return SEG_ARG_ERROR;

    if (ctx->encode_cb == NULL)
    {
        return SEG_NG;
    }
    else
    {
        uint32_t temp[SEG_DIGIT_LENGTH] = {0};
        uint32_t pattern[SEG_DIGIT_LENGTH] = {0};
        uint8_t length = ctx->encode_cb(text, temp);

        if (length > ctx->len)
        {
            /* エンコード後の文字数がセグメントの桁数を超える場合は、セグメントの桁数に合わせる */
            memcpy(&pattern[0], &temp[0], ctx->len * sizeof(uint32_t));
        }
        else if (length < ctx->len)
        {
            /* エンコード後の文字数がセグメントの桁数より少ない場合は、先頭を空白とする */
            memcpy(&pattern[ctx->len - length], &temp[0], length * sizeof(uint32_t));
        }
        else
        {
            /* エンコード後の文字数がセグメントの桁数と同数の場合は、そのままコピー */
            memcpy(&pattern[0], &temp[0], length * sizeof(uint32_t));
        }

        if (length > 0)
        {
            for (size_t i = 0; i < ctx->len; i++)
            {
                /* bitパターン設定 */
                seg_set_pattern(ctx, (i + 1), pattern[i]);
            }
        }
    }

    return SEG_OK;
}

/***
 * @brief bitパターン設定
 * @param ctx セグメント状態のメモリ領域
 * @param digit 桁位置(1～) / 0は全桁
 * @param pattern bitパターン
 */
int seg_set_pattern(seg_ctx_t *ctx, uint8_t digit, uint32_t pattern)
{
    if (ctx == NULL) return SEG_ARG_ERROR;

    if (digit == 0)
    {
        /* 全桁の点滅設定 */
        for (size_t i = 0; i < ctx->len; i++)
        {
            ctx->digit[i].pattern = pattern;
        }
    }
    else
    {
        uint8_t index = (digit - 1);

        if (index < ctx->len)
        {
            /* 指定桁の点滅設定 */
            ctx->digit[index].pattern = pattern;
        }
    }

    return SEG_OK;
}

/***
 * @brief ダイナミック制御モードへの移行
 */
int seg_set_dynamic_control(seg_ctx_t *ctx)
{
    if (ctx == NULL) return SEG_ARG_ERROR;

    seg_enter_dynamic_mode(ctx);

    return SEG_OK;
}

/***
 * @brief スタティック制御モードへの移行
 */
int seg_set_static_control(seg_ctx_t *ctx)
{
    if (ctx == NULL) return SEG_ARG_ERROR;

    seg_enter_static_mode(ctx);

    return SEG_OK;
}

/***
 * @brief ダイナミック制御モードへの移行
 */
static void seg_enter_dynamic_mode(seg_ctx_t *ctx)
{
    /* ダイナミック制御用パラメータ初期化 */
    ctx->control = SEG_DYNAMIC_CONTROL;
    ctx->scan_cycle = SEG_SCAN_CYCLE_DEFAULT_MS;
    ctx->last_scan  = ctx->timer;
    ctx->pos        = 0;
}

/***
 * @brief スタティック制御モードへの移行
 */
static void seg_enter_static_mode(seg_ctx_t *ctx)
{
    /* スタティック制御用パラメータ初期化 */
    ctx->control = SEG_STATIC_CONTROL;
}

/***
 * @brief 点滅設定処理
 */
int seg_hidden_on(seg_ctx_t *ctx, uint16_t digit)
{
    if (ctx == NULL) return SEG_ARG_ERROR;

    if (digit == 0)
    {
        /* 全桁の点滅設定 */
        for (size_t i = 0; i < ctx->len; i++)
        {
            ctx->digit[i].state = SEG_STATE_HIDDEN;
        }
    }
    else
    {
        uint8_t index = (digit - 1);

        if (index < ctx->len)
        {
            /* 指定桁の点滅設定 */
            ctx->digit[index].state = SEG_STATE_HIDDEN;
        }
    }

    return SEG_OK;
}

/***
 * @brief 点滅設定処理
 */
int seg_hidden_off(seg_ctx_t *ctx, uint16_t digit)
{
    if (ctx == NULL) return SEG_ARG_ERROR;

    if (digit == 0)
    {
        /* 全桁の点滅設定 */
        for (size_t i = 0; i < ctx->len; i++)
        {
            ctx->digit[i].state = SEG_STATE_COMMON;
        }
    }
    else
    {
        uint8_t index = (digit - 1);

        if (index < ctx->len)
        {
            /* 指定桁の点滅設定 */
            ctx->digit[index].state = SEG_STATE_COMMON;
        }
    }

    return SEG_OK;
}

/***
 * @brief 点滅設定処理
 * @param ctx セグメント状態のメモリ領域
 * @param digit 桁位置(1～) / 0は全桁
 * @param cycle 点滅周期
 */
int seg_blink_on(seg_ctx_t *ctx, uint16_t digit, uint32_t cycle)
{
    if ((ctx == NULL) || (cycle == 0)) return SEG_ARG_ERROR;

    if (digit == 0)
    {
        /* 全桁の点滅設定 */
        for (size_t i = 0; i < ctx->len; i++)
        {
            ctx->digit[i].state = SEG_STATE_BLINK;
            ctx->digit[i].blink_cycle = cycle;
        }
    }
    else
    {
        uint8_t index = (digit - 1);

        if (index < ctx->len)
        {
            /* 指定桁の点滅設定 */
            ctx->digit[index].state = SEG_STATE_BLINK;
            ctx->digit[index].blink_cycle = cycle;
        }
    }

    return SEG_OK;
}

/***
 * @brief 点滅設定処理
 * @param ctx セグメント状態のメモリ領域
 * @param digit 桁位置(1～) / 0は全桁
 */
int seg_blink_off(seg_ctx_t *ctx, uint16_t digit)
{
    if (ctx == NULL) return SEG_ARG_ERROR;

    if (digit == 0)
    {
        /* 全桁の点滅設定 */
        for (size_t i = 0; i < ctx->len; i++)
        {
            ctx->digit[i].state = SEG_STATE_COMMON;
        }
    }
    else
    {
        uint8_t index = (digit - 1);

        if (index < ctx->len)
        {
            /* 指定桁の点滅設定 */
            ctx->digit[index].state = SEG_STATE_COMMON;
        }
    }

    return SEG_OK;
}

/***
 * @brief スキャン周期設定処理
 * @param ctx セグメント状態のメモリ領域
 * @param value 設定値
 */
int seg_set_scan_cycle(seg_ctx_t *ctx, uint32_t value)
{
    if (ctx == NULL) return SEG_ARG_ERROR;

    if (value > 0)
    {
        ctx->scan_cycle = value;
    }

    return SEG_OK;
}
