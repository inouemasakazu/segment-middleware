/****************************************************************************************************
 * @file    display.c
 * @brief   セグメントLED表示制御用プログラム
 * @details このファイルにはセグメントLEDの表示制御機能を提供するモジュールを定義。
 *
 * @author  Masakazu Inoue
 * @date    2026/05/25          新規作成
 ****************************************************************************************************
 * @note
 * セグメントLEDの表示制御に必要な関数を定義する。
 ****************************************************************************************************/

/****************************************************************************************************
 * Private include
 ****************************************************************************************************/
#include "display.h"

#include "u74hc595/_u74hc595.h"

/***
 * @brief HALドライバー用include
 */
#include "stm32f446xx.h"
#include "stm32f4xx_hal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/****************************************************************************************************
 * Private define
 ****************************************************************************************************/
/*** セグメントの桁位置(GPIO pin指定用のマクロ) ***/
#define DISPLAY_DIGIT_1         GPIO_PIN_7
#define DISPLAY_DIGIT_2         GPIO_PIN_8
#define DISPLAY_DIGIT_3         GPIO_PIN_9
#define DISPLAY_DIGIT_4         GPIO_PIN_6

/****************************************************************************************************
 * Private typedef
 ****************************************************************************************************/

/****************************************************************************************************
 * Private Global Variables
 ****************************************************************************************************/
segdisp_t segment;

/****************************************************************************************************
 * Private Prototype Declaration
 ****************************************************************************************************/
static void draw_cb(const uint32_t *data, uint8_t len, uint8_t pos);
static uint8_t encode_cb(const uint8_t *c, uint32_t *pattern);
static void digit_position_control(uint8_t position);

/***
 * @brief 表示初期化
 */
void display_init(void)
{
    /* セグメントLED制御データ初期化 */
    segdisp_init(&segment, 4);
    segdisp_set_dynamic_control(&segment);      /* ダイナミック制御 */

    /* 描画コールバックとエンコードコールバックの設定 */
    segdisp_set_draw_cb(&segment, draw_cb);
    segdisp_set_encode_cb(&segment, encode_cb);

    /* "----"と描画するためのbitパターンを設定 */
    segdisp_set_pattern(&segment, 1, 0x40);
    segdisp_set_pattern(&segment, 2, 0x40);
    segdisp_set_pattern(&segment, 3, 0x40);
    segdisp_set_pattern(&segment, 4, 0x40);

    /* 点滅ON */
    segdisp_blink_on(&segment, 0, 500);         /* blink 500ms cycle */
}

/***
 * @brief 表示更新割り込み
 * @details タイマ割り込みなどで呼び出す。
 *          表示更新周期は1msを推奨。
 * 
 * @details 表示更新の流れ
 * 1. segdisp_update関数を周期起動する。
 * 2. 更新周期ごとに描画コールバック関数(draw_cb)が呼び出される。
 * 3. 描画コールバック関数内で点灯桁位置制御関数(digit_position_control)を呼び出し、点灯桁位置を制御する。
 * 4. 描画コールバック関数内でシフトレジスタへの点灯表示出力関数(_u74hc595_serial_output)を呼び出し、点灯表示を出力する。
 */
void display_interrupt(void)
{
    /* 表示更新 */
    segdisp_update(&segment, 1);    /* 1ms period inc */
}

/***
 * @brief 描画コールバック
 */
static void draw_cb(const uint32_t *data, uint8_t len, uint8_t pos)
{
    uint8_t byte = (uint8_t)*data;

    /* 点灯桁位置制御 */
    digit_position_control(pos);

    /* 点灯表示出力(HWはシフトレジスタ) */
    _u74hc595_serial_output(&byte, len, 0);
}

/***
 * @brief エンコードコールバック
 * @details 7segment led用のbitパターンを返す。数字以外は例外扱いで全消灯とする。
 *          小数点はbitパターンの8bit目を点灯とする。
 *          例) "1.23" -> {0x86, 0x3F, 0x5B, 0x00}
 * 
 * @param c エンコード対象の文字列を指すメモリ領域
 * @param pattern エンコード後のbitパターンを格納する配列
 * @return エンコード後の文字数
 */
static uint8_t encode_cb(const uint8_t *c, uint32_t *pattern)
{
    /* 7segment led用bitパターン */
    uint32_t num_bit[] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
    };

    uint8_t length = 0;

    for (size_t i = 0; i < SEGDISP_DIGIT_MAX; i++)
    {
        if (c[i] == '\0')
        {
            /* 文字列終端 */
            break;
        }
        else
        {
            /* パターン変換 */
            if (('0' <= c[i]) && (c[i] <= '9'))
            {
                /* 数字はbitパターンに変換して格納 */
                pattern[length] = num_bit[(c[i] - '0')];
                length++;
            }
            else if (c[i] == '.')
            {
                /* 小数点はbitパターンの8bit目を点灯 */
                if (length == 0)
                {
                    pattern[length] |= 0x80;
                    length++;
                }
                else
                {
                    pattern[(length - 1)] |= 0x80;
                }
            }
            else
            {
                /* 数字じゃない場合は例外扱い */
                pattern[length] = 0x00;
                length++;
            }
        }
    }

    return length;
}

/***
 * @brief 点灯桁制御(ダイナミック制御用)
 */
static void digit_position_control(uint8_t position)
{
    GPIO_PinState light[][4] = {
        /* 端子はLowアクティブ */
        {GPIO_PIN_RESET, GPIO_PIN_SET,   GPIO_PIN_SET,   GPIO_PIN_SET  },       /* 1桁目点灯 */
        {GPIO_PIN_SET,   GPIO_PIN_RESET, GPIO_PIN_SET,   GPIO_PIN_SET  },       /* 2桁目点灯 */
        {GPIO_PIN_SET,   GPIO_PIN_SET,   GPIO_PIN_RESET, GPIO_PIN_SET  },       /* 3桁目点灯 */
        {GPIO_PIN_SET,   GPIO_PIN_SET,   GPIO_PIN_SET,   GPIO_PIN_RESET}        /* 4桁目点灯 */
    };

    position = position % 4;    /* 桁数は0-3でループ */

    HAL_GPIO_WritePin(GPIOA, DISPLAY_DIGIT_1, light[position][0]);
    HAL_GPIO_WritePin(GPIOB, DISPLAY_DIGIT_2, light[position][1]);
    HAL_GPIO_WritePin(GPIOB, DISPLAY_DIGIT_3, light[position][2]);
    HAL_GPIO_WritePin(GPIOA, DISPLAY_DIGIT_4, light[position][3]);
}