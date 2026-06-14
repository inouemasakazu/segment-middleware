/****************************************************************************************************
 * @file    segment.h
 * @brief   UARTデバイスドライバ
 * @details 外部公開するAPI、型定義、マクロを定義する
 *
 * @author  Masakazu Inoue
 * @date    2026/05/25          新規作成
 ****************************************************************************************************/
#ifndef __SEGMENT_H__
#define __SEGMENT_H__

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
 * @brief 使用するセグメントの桁長
 */
#define SEG_DIGIT_LENGTH            16

/***
 * @brief ダイナミック制御時のscan更新周期
 */
#define SEG_SCAN_CYCLE_DEFAULT_MS   2

/****************************************************************************************************
 * Public typedef
 ****************************************************************************************************/

typedef enum
{
    SEG_STATE_COMMON = 0,
    SEG_STATE_HIDDEN,
    SEG_STATE_BLINK
} seg_state_e;

typedef void (*segment_draw_cb_t)(const uint32_t *data, uint8_t len, uint8_t pos);
typedef uint8_t (*segment_encode_cb_t)(const uint8_t *c, uint32_t *pattern);

typedef struct
{
    uint32_t pattern;

    seg_state_e state;      /* 描画状態 */
    uint32_t blink_cycle;
} seg_digit_t;

typedef struct
{
    seg_digit_t digit[SEG_DIGIT_LENGTH];
    uint8_t len;
    uint8_t pos;

    uint16_t control;

    uint32_t timer;
    uint32_t scan_cycle;
    uint32_t last_scan;

    /* callback func */
    segment_draw_cb_t draw_cb;
    segment_encode_cb_t encode_cb;
} seg_ctx_t;

/****************************************************************************************************
 * Public Global Variables
 ****************************************************************************************************/

/****************************************************************************************************
 * Public Prototype Declaration
 ****************************************************************************************************/
int seg_init(seg_ctx_t *ctx, uint8_t length);

int seg_set_draw_cb(seg_ctx_t *ctx, segment_draw_cb_t cb);
int seg_set_encode_cb(seg_ctx_t *ctx, segment_encode_cb_t cb);

int seg_set_static_control(seg_ctx_t *ctx);
int seg_set_dynamic_control(seg_ctx_t *ctx);

/***
 * @brief 状態・描画の更新
 */
int seg_update(seg_ctx_t *ctx, uint32_t period);

int seg_set_text(seg_ctx_t *ctx, const uint8_t *text);
int seg_set_pattern(seg_ctx_t *ctx, uint8_t digit, uint32_t pattern);

int seg_hidden_on(seg_ctx_t *ctx, uint16_t digit);
int seg_hidden_off(seg_ctx_t *ctx, uint16_t digit);

int seg_blink_on(seg_ctx_t *ctx, uint16_t digit, uint32_t cycle);
int seg_blink_off(seg_ctx_t *ctx, uint16_t digit);

int seg_set_scan_cycle(seg_ctx_t *ctx, uint32_t value);

#endif  /* __SEGMENT_H__ */