/****************************************************************************************************
 * @file    segdisp.h
 * @brief   セグメントディスプレイ制御用モジュール
 * @details 外部公開するAPI、型定義、マクロを定義する
 *
 * @author  Masakazu Inoue
 * @date    2026/05/25          新規作成
 ****************************************************************************************************/
#ifndef __SEGDISP_H__
#define __SEGDISP_H__

/****************************************************************************************************
 * Public include
 ****************************************************************************************************/
#include <stdint.h>

/****************************************************************************************************
 * Public define
 ****************************************************************************************************/
/***
 * @brief 処理結果
 */
#define SEG_OK                      0       /* 処理成功 */
#define SEG_NG                      1       /* 処理失敗 */
#define SEG_ARG_ERROR              -1       /* 引数異常 */

/***
 * @brief セグメントディスプレイの桁数
 */
#define SEGDISP_DIGIT_MAX           16

/***
 * @brief ダイナミック制御時のscan更新周期
 */
#define SEGDISP_SCAN_CYCLE_DEFAULT_MS   2

/****************************************************************************************************
 * Public typedef
 ****************************************************************************************************/

typedef enum
{
    SEGDISP_CONTROL_STATIC = 0,
    SEGDISP_CONTROL_DYNAMIC
} segdisp_control_e;

typedef enum
{
    SEGDISP_STATE_COMMON = 0,
    SEGDISP_STATE_HIDDEN,
    SEGDISP_STATE_BLINK
} segdisp_state_e;

typedef void (*draw_cb_t)(const uint32_t *data, uint8_t len, uint8_t pos);
typedef uint8_t (*encode_cb_t)(const uint8_t *c, uint32_t *pattern);

typedef struct
{
    uint32_t pattern;

    segdisp_state_e state;      /* 描画状態 */
    uint32_t blink_cycle;
} digit_t;

typedef struct
{
    digit_t digit[SEGDISP_DIGIT_MAX];
    uint8_t len;
    uint8_t pos;

    segdisp_control_e control;

    uint32_t timer;
    uint32_t scan_cycle;
    uint32_t last_scan;

    /* callback func */
    draw_cb_t draw_cb;
    encode_cb_t encode_cb;
} segdisp_t;

/****************************************************************************************************
 * Public Global Variables
 ****************************************************************************************************/

/****************************************************************************************************
 * Public Prototype Declaration
 ****************************************************************************************************/

/**
 * @brief セグメントディスプレイ初期化
 * @param segdisp セグメントディスプレイの制御データを保持するメモリ領域
 * @param segment セグメント数
 * @param digit   桁数
 * @return 処理結果
 */
int segdisp_init(segdisp_t *segdisp, uint8_t segment, uint8_t digit);

int segdisp_set_draw_cb(segdisp_t *segdisp, draw_cb_t cb);
int segdisp_set_encode_cb(segdisp_t *segdisp, encode_cb_t cb);

/**
 * @brief 制御方式の設定
 * @param control 制御方式(STATIC or DYNAMIC)
 * @return 処理結果
 */
int segdisp_set_control(segdisp_t *segdisp, segdisp_control_e control);

/***
 * @brief 状態・描画の更新
 */
int segdisp_update(segdisp_t *segdisp, uint32_t period);

int segdisp_set_text(segdisp_t *segdisp, const uint8_t *text);
int segdisp_set_pattern(segdisp_t *segdisp, uint8_t digit, uint32_t pattern);

int segdisp_hidden_on(segdisp_t *segdisp, uint16_t digit);
int segdisp_hidden_off(segdisp_t *segdisp, uint16_t digit);

int segdisp_blink_on(segdisp_t *segdisp, uint16_t digit, uint32_t cycle);
int segdisp_blink_off(segdisp_t *segdisp, uint16_t digit);

int segdisp_set_scan_cycle(segdisp_t *segdisp, uint32_t value);

#endif  /* __SEGDISP_H__ */