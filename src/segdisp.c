/****************************************************************************************************
 * @file    segdisp.c
 * @brief   セグメントディスプレイ制御用モジュール
 * @details このファイルではセグメントディスプレイ用ミドルウェアモジュールを定義。
 *
 * @author  Masakazu Inoue
 * @date    2026/05/25          新規作成
 ****************************************************************************************************/

/****************************************************************************************************
 * Private include
 ****************************************************************************************************/
#include "../inc/segdisp.h"

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


/****************************************************************************************************
 * Private typedef
 ****************************************************************************************************/

/****************************************************************************************************
 * Private Global Variables
 ****************************************************************************************************/

/****************************************************************************************************
 * Private Prototype Declaration
 ****************************************************************************************************/
static void segdisp_call_draw_cb(segdisp_t *segdisp, uint8_t length, uint8_t position);

static void segdisp_enter_dynamic_mode(segdisp_t *segdisp);
static void segdisp_enter_static_mode(segdisp_t *segdisp);

/**
 * @brief セグメントディスプレイ初期化
 * @param segdisp セグメントディスプレイの制御データを保持するメモリ領域
 * @param segment セグメント数
 * @param digit   桁数
 * @return 処理結果
 */
int segdisp_init(segdisp_t *segdisp, uint8_t segment, uint8_t digit)
{
    int success = SEG_OK;

    if ((segdisp == NULL)
    ||  ((0 == digit) || (SEGDISP_DIGIT_MAX <= digit)))
    {
        success = SEG_ARG_ERROR;
    }
    else
    {
        /* メモリ0クリア */
        memset(segdisp, 0, sizeof(segdisp_t));

        /* 制御方式設定(デフォルトはSTATIC) */
        segdisp_set_control(segdisp, SEGDISP_CONTROL_STATIC);

        segdisp->len = digit;
    }

    return success;
}

/***
 * @brief  描画コールバック登録
 * @param  segdisp セグメント状態のメモリ領域
 * @param  cb  コールバック関数のメモリ領域
 * @return 処理結果
 */
int segdisp_set_draw_cb(segdisp_t *segdisp, draw_cb_t cb)
{
    if (segdisp == NULL) return SEG_ARG_ERROR;

    segdisp->draw_cb = cb;

    return SEG_OK;
}

/***
 * @brief  エンコードコールバック登録
 * @param  segdisp セグメント状態のメモリ領域
 * @param  cb  コールバック関数のメモリ領域
 * @return 処理結果
 */
int segdisp_set_encode_cb(segdisp_t *segdisp, encode_cb_t cb)
{
    if (segdisp == NULL) return SEG_ARG_ERROR;

    segdisp->encode_cb = cb;

    return SEG_OK;
}

/***
 * @brief  更新処理
 * @param  segdisp セグメント状態のメモリ領域
 * @param  period 内部タイマーの更新周期
 * @return 処理結果
 */
int segdisp_update(segdisp_t *segdisp, uint32_t period)
{
    uint8_t len = 0;
    uint8_t pos = 0;
    bool update = SEG_UPDATE_ON;

    if (segdisp == NULL) return SEG_ARG_ERROR;

    segdisp->timer += period;

    if (SEGDISP_CONTROL_DYNAMIC == segdisp->control)
    {
        /* ダイナミック制御は描画更新のため周期判定を行う */
        if ((segdisp->timer - segdisp->last_scan) >= segdisp->scan_cycle)
        {
            segdisp->last_scan += segdisp->scan_cycle;

            len = 1;
            pos = segdisp->pos;
            segdisp->pos = (segdisp->pos + 1) % segdisp->len;   /* scan位置の更新 */
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
        len = segdisp->len;
    }

    if (update == SEG_UPDATE_ON)
    {
        /* 描画更新 */
        segdisp_call_draw_cb(segdisp, len, pos);
    }

    return SEG_OK;
}

/***
 * @brief 描画コールバック呼び出し
 *        描画データの生成、及びコールバック処理の起動
 * @param segdisp セグメント状態のメモリ領域
 * @param length 描画データ長
 * @param position 描画データ位置
 */
static void segdisp_call_draw_cb(segdisp_t *segdisp, uint8_t length, uint8_t position)
{
    uint32_t buf[SEGDISP_DIGIT_MAX];

    bool is_visible;

    for (size_t i = 0; i < length; i++)
    {
        switch (segdisp->digit[(position + i)].state)
        {
        case SEGDISP_STATE_HIDDEN:
            /* 非表示設定 */
            is_visible = SEG_VISIBLE_OFF;
            break;

        case SEGDISP_STATE_BLINK:
            /* 点滅設定 */
            is_visible = (((segdisp->timer / segdisp->digit[(position + i)].blink_cycle) % 2) == 0);
            break;

        case SEGDISP_STATE_COMMON:
        default:
            /* 表示状態の指定なし */
            is_visible = SEG_VISIBLE_ON;
            break;
        }

        buf[i] = is_visible ? segdisp->digit[(position + i)].pattern : 0;
    }

    if (segdisp->draw_cb != NULL)
    {
        /* 描画コールバック呼び出し */
        segdisp->draw_cb(&buf[0], length, position);
    }
}

/***
 * @brief  テキスト設定
 * @param segdisp セグメント状態のメモリ領域
 * @param text 設定するテキストのメモリ領域
 * @return 処理結果
 */
int segdisp_set_text(segdisp_t *segdisp, const uint8_t *text)
{
    if ((segdisp == NULL) || (*text == '\0')) return SEG_ARG_ERROR;

    if (segdisp->encode_cb == NULL)
    {
        return SEG_NG;
    }
    else
    {
        uint32_t temp[SEGDISP_DIGIT_MAX] = {0};
        uint32_t pattern[SEGDISP_DIGIT_MAX] = {0};
        uint8_t length = segdisp->encode_cb(text, temp);

        if (length > segdisp->len)
        {
            /* エンコード後の文字数がセグメントの桁数を超える場合は、セグメントの桁数に合わせる */
            memcpy(&pattern[0], &temp[0], segdisp->len * sizeof(uint32_t));
        }
        else if (length < segdisp->len)
        {
            /* エンコード後の文字数がセグメントの桁数より少ない場合は、先頭を空白とする */
            memcpy(&pattern[segdisp->len - length], &temp[0], length * sizeof(uint32_t));
        }
        else
        {
            /* エンコード後の文字数がセグメントの桁数と同数の場合は、そのままコピー */
            memcpy(&pattern[0], &temp[0], length * sizeof(uint32_t));
        }

        if (length > 0)
        {
            for (size_t i = 0; i < segdisp->len; i++)
            {
                /* bitパターン設定 */
                segdisp_set_pattern(segdisp, (i + 1), pattern[i]);
            }
        }
    }

    return SEG_OK;
}

/***
 * @brief bitパターン設定
 * @param segdisp セグメント状態のメモリ領域
 * @param digit 桁位置(1～) / 0は全桁
 * @param pattern bitパターン
 */
int segdisp_set_pattern(segdisp_t *segdisp, uint8_t digit, uint32_t pattern)
{
    if (segdisp == NULL) return SEG_ARG_ERROR;

    if (digit == 0)
    {
        /* 全桁の点滅設定 */
        for (size_t i = 0; i < segdisp->len; i++)
        {
            segdisp->digit[i].pattern = pattern;
        }
    }
    else
    {
        uint8_t index = (digit - 1);

        if (index < segdisp->len)
        {
            /* 指定桁の点滅設定 */
            segdisp->digit[index].pattern = pattern;
        }
    }

    return SEG_OK;
}

/**
 * @brief 制御方式の設定
 * @param control 制御方式(STATIC or DYNAMIC)
 * @return 処理結果
 */
int segdisp_set_control(segdisp_t *segdisp, segdisp_control_e control)
{
    int success = SEG_OK;

    if (segdisp == NULL)
    {
        success = SEG_ARG_ERROR;
    }
    else
    {
        /* 制御方式設定 */
        switch (control)
        {
        case SEGDISP_CONTROL_STATIC:
            segdisp_enter_static_mode(segdisp);
            break;

        case SEGDISP_CONTROL_DYNAMIC:
            segdisp_enter_dynamic_mode(segdisp);
            break;

        default:
            success = SEG_NG;
            break;
        }
    }

    return success;
}

/***
 * @brief ダイナミック制御モードへの移行
 */
static void segdisp_enter_dynamic_mode(segdisp_t *segdisp)
{
    /* ダイナミック制御用パラメータ初期化 */
    segdisp->control = SEGDISP_CONTROL_DYNAMIC;

    segdisp->pos        = 0;
    segdisp->scan_cycle = SEGDISP_SCAN_CYCLE_DEFAULT_MS;
    segdisp->last_scan  = segdisp->timer;
}

/***
 * @brief スタティック制御モードへの移行
 */
static void segdisp_enter_static_mode(segdisp_t *segdisp)
{
    /* スタティック制御用パラメータ初期化 */
    segdisp->control = SEGDISP_CONTROL_STATIC;
}

/***
 * @brief 点滅設定処理
 */
int segdisp_hidden_on(segdisp_t *segdisp, uint16_t digit)
{
    if (segdisp == NULL) return SEG_ARG_ERROR;

    if (digit == 0)
    {
        /* 全桁の点滅設定 */
        for (size_t i = 0; i < segdisp->len; i++)
        {
            segdisp->digit[i].state = SEGDISP_STATE_HIDDEN;
        }
    }
    else
    {
        uint8_t index = (digit - 1);

        if (index < segdisp->len)
        {
            /* 指定桁の点滅設定 */
            segdisp->digit[index].state = SEGDISP_STATE_HIDDEN;
        }
    }

    return SEG_OK;
}

/***
 * @brief 点滅設定処理
 */
int segdisp_hidden_off(segdisp_t *segdisp, uint16_t digit)
{
    if (segdisp == NULL) return SEG_ARG_ERROR;

    if (digit == 0)
    {
        /* 全桁の点滅設定 */
        for (size_t i = 0; i < segdisp->len; i++)
        {
            segdisp->digit[i].state = SEGDISP_STATE_COMMON;
        }
    }
    else
    {
        uint8_t index = (digit - 1);

        if (index < segdisp->len)
        {
            /* 指定桁の点滅設定 */
            segdisp->digit[index].state = SEGDISP_STATE_COMMON;
        }
    }

    return SEG_OK;
}

/***
 * @brief 点滅設定処理
 * @param segdisp セグメント状態のメモリ領域
 * @param digit 桁位置(1～) / 0は全桁
 * @param cycle 点滅周期
 */
int segdisp_blink_on(segdisp_t *segdisp, uint16_t digit, uint32_t cycle)
{
    if ((segdisp == NULL) || (cycle == 0)) return SEG_ARG_ERROR;

    if (digit == 0)
    {
        /* 全桁の点滅設定 */
        for (size_t i = 0; i < segdisp->len; i++)
        {
            segdisp->digit[i].state = SEGDISP_STATE_BLINK;
            segdisp->digit[i].blink_cycle = cycle;
        }
    }
    else
    {
        uint8_t index = (digit - 1);

        if (index < segdisp->len)
        {
            /* 指定桁の点滅設定 */
            segdisp->digit[index].state = SEGDISP_STATE_BLINK;
            segdisp->digit[index].blink_cycle = cycle;
        }
    }

    return SEG_OK;
}

/***
 * @brief 点滅設定処理
 * @param segdisp セグメント状態のメモリ領域
 * @param digit 桁位置(1～) / 0は全桁
 */
int segdisp_blink_off(segdisp_t *segdisp, uint16_t digit)
{
    if (segdisp == NULL) return SEG_ARG_ERROR;

    if (digit == 0)
    {
        /* 全桁の点滅設定 */
        for (size_t i = 0; i < segdisp->len; i++)
        {
            segdisp->digit[i].state = SEGDISP_STATE_COMMON;
        }
    }
    else
    {
        uint8_t index = (digit - 1);

        if (index < segdisp->len)
        {
            /* 指定桁の点滅設定 */
            segdisp->digit[index].state = SEGDISP_STATE_COMMON;
        }
    }

    return SEG_OK;
}

/**
 * @brief スキャン周期設定
 * @param value 周期
 */
int segdisp_set_scan_cycle(segdisp_t *segdisp, uint32_t value)
{
    if (segdisp == NULL) return SEG_ARG_ERROR;

    if (value > 0)
    {
        segdisp->scan_cycle = value;
    }

    return SEG_OK;
}
