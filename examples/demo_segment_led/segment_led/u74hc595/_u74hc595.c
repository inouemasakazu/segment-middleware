/****************************************************************************************************
 * @file    _u74hc595.c
 * @brief   8bit シリアル入力シフトレジスタ制御用プログラム
 * @details このファイルには8bit シリアル入力シフトレジスタの制御機能を提供するモジュールを定義。
 *
 * @author  Masakazu Inoue
 * @date    2026/03/11          新規作成
 ****************************************************************************************************
 * @note
 * シフトレジスタの制御に必要な関数を定義する。
 ****************************************************************************************************/

/****************************************************************************************************
 * Private include
 ****************************************************************************************************/
#include "_u74hc595.h"

#include "stm32f4xx_hal.h"

#include <stddef.h>

/****************************************************************************************************
 * Private define
 ****************************************************************************************************/

/****************************************************************************************************
 * Private typedef
 ****************************************************************************************************/

/****************************************************************************************************
 * Private Global Variables
 ****************************************************************************************************/

/****************************************************************************************************
 * Private Prototype Declaration
 ****************************************************************************************************/
static void _u74hc595_write_SER( unsigned char value );
static void _u74hc595_write_OE( unsigned char level );
static void _u74hc595_write_RCLK( unsigned char level );
static void _u74hc595_write_SRCLK( unsigned char level );
static void _u74hc595_write_SRCLR( unsigned char level );

/***
 * @brief シリアル出力(カスケード接続対応)
 * @param value 出力データのメモリ領域
 * @param size  出力データサイズ[byte](1～)
 * @param endian エンディアン (0:ビッグエンディアン, 1:リトルエンディアン)
 * @return なし
 *
 * @note
 * 最小1byte(8bit)のデータをシフトレジスタへシリアル出力する。
 * 出力後はストレージレジスタへ転送し、出力イネーブルする。
 * 
 * 複数のシフトレジスタをカスケード接続している場合は、
 * 出力データをシフトレジスタの数×8bit分用意し、sizeにシフトレジスタの数を指定する。
 */
void _u74hc595_serial_output( unsigned char *value, unsigned short size, unsigned char endian )
{
    unsigned char bit;
    unsigned char byte;

    _u74hc595_write_RCLK( 0 );

    for (size_t i = 0; i < size; i++)
    {
        byte = *value++;

        for (size_t j = 0; j < 8; j++)
        {
            if (endian == 0)
            {
                /* シリアル出力はビッグエンディアンで行う場合は、上位ビットから出力する */
                bit = (byte >> (7 - j)) & 0x1;
            }
            else
            {
                /* シリアル出力はリトルエンディアンで行う場合は、下位ビットから出力する */
                bit = (byte >> j) & 0x1;
            }

            _u74hc595_write_SRCLK( 0 );
            _u74hc595_write_SER( bit );          /* シフトレジスタへ出力 */
            _u74hc595_write_SRCLK( 1 );
        }
    }

    _u74hc595_write_RCLK( 1 );      /* ストレージレジスタへ転送 */
    _u74hc595_write_OE( 0 );        /* 出力イネーブル */
}

/***
 * @brief シフトレジスタの保持データをクリアする
 * @details シフトレジスタの保持データをクリアするためにSRCLR端子をLowにする。
 *          クリア後の端子levelはHigh固定。
 */
void _u74hc595_storage_clear( void )
{
    _u74hc595_write_SRCLR( 0 );

    /* 端子Level変化用の遅延 */
    for (size_t delay = 0; delay <= 0xff; delay++);

    _u74hc595_write_SRCLR( 1 );     /* クリア後の端子LevelはHigh固定 */
}

/*** シリアル入力端子 ***/
static void _u74hc595_write_SER( unsigned char value )
{
    HAL_GPIO_WritePin( GPIOC, GPIO_PIN_1, (GPIO_PinState)value );
}

/*** アウトプットイネーブル ***/
static void _u74hc595_write_OE( unsigned char level )
{
    ;
}

/*** ラッチ信号 ***/
static void _u74hc595_write_RCLK( unsigned char level )
{
    HAL_GPIO_WritePin( GPIOC, GPIO_PIN_3, (GPIO_PinState)level );
}

/*** シフトクロック入力端子 ***/
static void _u74hc595_write_SRCLK( unsigned char level )
{
    HAL_GPIO_WritePin( GPIOC, GPIO_PIN_2, (GPIO_PinState)level );
}

/*** 非同期クリア端子 ***/
static void _u74hc595_write_SRCLR( unsigned char level )
{
    ;
}