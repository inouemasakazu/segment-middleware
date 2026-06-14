/****************************************************************************************************
 * @file    uart.c
 * @brief   UARTデバイスドライバ
 * @details このファイルにはUARTデバイスドライバ機能を提供するモジュールを定義。
 *
 * @author  Masakazu Inoue
 * @date    2026/mm/dd     Create New.
 ****************************************************************************************************
 * @note
 * uart.hにて外部公開しているAPIのみでUART機能が使用可能。
 * UART通信(送信/受信)を行う場合は先に開局処理を行う。開局後に通信は可能となる。
 ****************************************************************************************************/

/****************************************************************************************************
 * Private include
 ****************************************************************************************************/
#include "uart.h"
#include "uart_config.h"

#include <string.h>

/***
 * @brief HALドライバー用include
 */
#include "stm32f446xx.h"
#include "stm32f4xx_hal.h"

/****************************************************************************************************
 * Private define
 ****************************************************************************************************/
/***
 * @brief UART処理の実行結果
 */
#define UART_OK             0       /* 処理成功 */
#define UART_NG             1       /* 処理失敗 */
#define UART_ARG_ERROR     -1       /* 引数異常 */

/***
 * @brief 送信状態
 */
#define UART_TX_OFF         0       /* 送信なし */
#define UART_TX_ON          1       /* 送信中 */

/***
 * @brief PORTの使用状態
 */
#define UART_PORT_CLOSE     0       /* PORT閉局 */
#define UART_PORT_OPEN      1       /* PORT開局 */

/***
 * @brief UARTのTX/RXピンのピン番号
 */
#define UART_PORT0_TX_PIN   GPIO_PIN_2
#define UART_PORT0_RX_PIN   GPIO_PIN_3

#define UART_PORT1_TX_PIN   GPIO_PIN_9
#define UART_PORT1_RX_PIN   GPIO_PIN_10

#define UART_PORT2_TX_PIN   GPIO_PIN_10
#define UART_PORT2_RX_PIN   GPIO_PIN_5

/****************************************************************************************************
 * Private typedef
 ****************************************************************************************************/
/***
 * @brief UART構造
 */
typedef struct
{
    unsigned char state;        /* this state CLOSE or OPEN */

    UART_HandleTypeDef handle;
    void * upper_ctx;

    /* Callback handler. */
    void (* tx_callback)(void * ctx);
    void (* rx_callback)(void * ctx, unsigned char data);

    volatile unsigned char tx_busy;
    unsigned char rx_byte;          /* 1byte buf */
} uart_obj_t;

/****************************************************************************************************
 * Private Variables
 ****************************************************************************************************/
static uart_obj_t uart_obj[UART_PORT_NUM];

static USART_TypeDef * instance[UART_PORT_NUM] = {
#if   UART_PORT_NUM == 1
    USART2
#elif UART_PORT_NUM == 2
    USART2,
    USART1
#elif UART_PORT_NUM == 3
    USART2,
    USART1,
    USART3
#endif
};

/****************************************************************************************************
 * Private Functions
 ****************************************************************************************************/
static uart_obj_t * uart_obj_find_handle(UART_HandleTypeDef * handle);
static uart_obj_t * uart_obj_find_ctx(void * ctx);

/***
 * @name  uart_open
 * @brief UART開局処理
 * @param port 開局対象のポート番号
 * @param ctx 上位コンテキスト
 * @param baudrate 通信速度
 * @return 処理結果
 */
int uart_open(unsigned short port, void * ctx, unsigned long baudrate)
{
    int success = UART_ARG_ERROR;

    if (port >= UART_PORT_NUM) return success;

    uart_obj_t * obj = &uart_obj[port];

    if (obj->state == UART_PORT_CLOSE)      /* 未OPEN? */
    {
        obj->handle.Init.BaudRate = baudrate;

        /* ボーレート以外は固定パラメータ */
        obj->handle.Instance          = instance[port];
        obj->handle.Init.WordLength   = UART_WORDLENGTH_8B;
        obj->handle.Init.StopBits     = UART_STOPBITS_1;
        obj->handle.Init.Parity       = UART_PARITY_NONE;
        obj->handle.Init.Mode         = UART_MODE_TX_RX;
        obj->handle.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
        obj->handle.Init.OverSampling = UART_OVERSAMPLING_16;
        if (HAL_UART_Init(&obj->handle) == HAL_OK)
        {
            success = UART_OK;

            obj->state       = UART_PORT_OPEN;
            obj->upper_ctx   = ctx;
            obj->tx_callback = NULL;
            obj->rx_callback = NULL;
            obj->tx_busy     = UART_TX_OFF;
            obj->rx_byte     = 0;

            /* 受信開始 */
            HAL_UART_Receive_IT(&obj->handle, &obj->rx_byte, 1);
        }
    }
    else
    {
        success = UART_NG;
    }

    return success;
}

/***
 * @name  uart_close
 * @brief UART閉局処理
 * @param port 閉局対象のポート番号
 * @return 処理結果
 */
int uart_close(unsigned short port)
{
    int success = UART_ARG_ERROR;

    if (port >= UART_PORT_NUM) return success;

    uart_obj_t * obj = &uart_obj[port];

    if ((obj->state == UART_PORT_OPEN) &&       /* OPEN済み? */
        (obj->tx_busy == UART_TX_OFF))          /* NOT送信中 */
    {
        if (HAL_UART_DeInit(&obj->handle) == HAL_OK)
        {
            success = UART_OK;

            obj->state       = UART_PORT_CLOSE;
            obj->upper_ctx   = NULL;
            obj->tx_callback = NULL;
            obj->rx_callback = NULL;
            obj->tx_busy     = UART_TX_OFF;
            obj->rx_byte     = 0;
        }
    }
    else
    {
        success = UART_NG;
    }

    return success;
}

/***
 * @name  uart_obj_find_handle
 * @brief ハンドラ検索
 *        objに登録しているHALハンドラに一致するポインタを返す
 * @param handle HALハンドラのメモリ領域
 * @return objのポインタ or NULL
 */
static uart_obj_t * uart_obj_find_handle(UART_HandleTypeDef * handle)
{
    uart_obj_t * obj;

    for (size_t i = 0; i < UART_PORT_NUM; i++)
    {
        obj = &uart_obj[i];

        if (&obj->handle == handle)
        {
            return obj;
        }
    }

    return NULL;
}

/***
 * @name  uart_obj_find_ctx
 * @brief コンテキスト検索
 *        objに登録している上位コンテキスト一致するポインタを返す
 * @param ctx 上位コンテキストのメモリ領域
 * @return objのポインタ or NULL
 */
static uart_obj_t * uart_obj_find_ctx(void * ctx)
{
    uart_obj_t * obj;

    for (size_t i = 0; i < UART_PORT_NUM; i++)
    {
        obj = &uart_obj[i];

        if (obj->upper_ctx == ctx)
        {
            return obj;
        }
    }

    return NULL;
}

/***
 * @name  uart_set_tx_callback
 * @brief UART送信完了時のコールバック関数設定
 * @param callback コールバックのメモリ領域
 */
void uart_set_tx_callback(void * ctx, void (* callback)(void *))
{
    uart_obj_t * obj = uart_obj_find_ctx(ctx);

    if (obj != NULL)
    {
        /* コールバックなし(引数がNULLでも)の場合でも許容する */
        obj->tx_callback = callback;
    }
    else
    {
        /* DO NOTHING */
    }
}

/***
 * @name  uart_set_rx_callback
 * @brief UART受信完了時のコールバック関数設定
 * @param callback コールバックのメモリ領域
 */
void uart_set_rx_callback(void * ctx, void (* callback)(void *, unsigned char))
{
    uart_obj_t * obj = uart_obj_find_ctx(ctx);

    if (obj != NULL)
    {
        /* コールバックなし(引数がNULLでも)の場合でも許容する */
        obj->rx_callback = callback;
    }
    else
    {
        /* DO NOTHING */
    }
}

/***
 * @name  uart_tx_start
 * @brief UART送信開始
 * @param pData 送信データのメモリ領域
 * @param size  送信データサイズ
 * @return 処理結果
 */
int uart_tx_start(void * ctx, unsigned char * pData, unsigned long size)
{
    int success = UART_ARG_ERROR;

    if ((pData == NULL) || (size == 0)) return success;    /* 引数異常 */

    uart_obj_t * obj = uart_obj_find_ctx(ctx);

    if (obj != NULL)
    {
        if (obj->tx_busy == UART_TX_OFF)
        {
            /* 送信開始 */
            if (HAL_UART_Transmit_IT(&obj->handle, pData, size) == HAL_OK)
            {
                obj->tx_busy = UART_TX_ON;

                success = UART_OK;
            }
            else
            {
                success = UART_NG;      /* 成功以外はすべてNGにする */
            }
        }
    }

    return success;
}

/***
 * @brief Tx Transfer completed callbacks.
 * 
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    uart_obj_t * obj = uart_obj_find_handle((UART_HandleTypeDef *)huart);

    if (obj != NULL)
    {
        obj->tx_busy = UART_TX_OFF;

        if (obj->tx_callback != NULL)
        {
            obj->tx_callback(obj->upper_ctx);
        }
    }
    else
    {
        /* DO NOTHING */
    }
}

/***
 * @brief Rx Transfer completed callbacks.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uart_obj_t * obj = uart_obj_find_handle((UART_HandleTypeDef *)huart);

    if (obj != NULL)
    {
        unsigned char data = obj->rx_byte;

        /* 再度、受信開始 */
        HAL_UART_Receive_IT(&obj->handle, &obj->rx_byte, 1);

        if (obj->rx_callback != NULL)
        {
            obj->rx_callback(obj->upper_ctx, data);
        }
    }
    else
    {
        /* DO NOTHING */
    }
}

/***
 * @name USART1_IRQHandler
 * @brief USART1の割り込みハンドラ
 */
void USART1_IRQHandler(void)
{
    uart_obj_t * obj = &uart_obj[UART_COM1];

    HAL_UART_IRQHandler(&obj->handle);
}

/***
 * @name USART2_IRQHandler
 * @brief USART2の割り込みハンドラ
 */
void USART2_IRQHandler(void)
{
    uart_obj_t * obj = &uart_obj[UART_COM_DEBUG];

    HAL_UART_IRQHandler(&obj->handle);
}

/***
 * @name USART3_IRQHandler
 * @brief USART3の割り込みハンドラ
 */
void USART3_IRQHandler(void)
{
    uart_obj_t * obj = &uart_obj[UART_COM2];

    HAL_UART_IRQHandler(&obj->handle);
}

/***
 * @name  HAL_UART_MspInit
 */
void HAL_UART_MspInit(UART_HandleTypeDef* huart)
{
    if (huart->Instance == USART2)
    {
        /* UART PORT0 GPIO pin Config. */
        uart_pin_init_port0();
    }
    else if (huart->Instance == USART1)
    {
        /* UART PORT1 GPIO pin Config. */
        uart_pin_init_port1();
    }
    else if (huart->Instance == USART3)
    {
        /* UART PORT2 GPIO pin Config. */
        uart_pin_init_port2();
    }
    else
    {
        /* DO NOTHING */

    }
}

/***
 * @name  HAL_UART_MspDeInit
 */
void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        /* PORT0 GPIO Configuration. */
        uart_pin_deinit_port0();
    }
    else if (huart->Instance == USART1)
    {
        /* PORT1 GPIO Configuration. */
        uart_pin_deinit_port1();
    }
    else if (huart->Instance == USART3)
    {
        /* PORT2 GPIO Configuration. */
        uart_pin_deinit_port2();
    }
    else
    {
        /* DO NOTHING */
    }
}

/***
 * @name uart_pin_init_port0
 * @brief 対象PORTのGPIOピン設定を有効化する
 */
void uart_pin_init_port0(void)
{
    GPIO_InitTypeDef gpio;

    /* Peripheral clock enable */
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* USART2 GPIO Configuration. */
    gpio.Pin       = (UART_PORT0_TX_PIN | UART_PORT0_RX_PIN);
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;

    /* gpio port init. */
    HAL_GPIO_Init(GPIOA, &gpio);

    /* interrupt init. */
    HAL_NVIC_SetPriority(USART2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

/***
 * @name uart_pin_init_port1
 * @brief 対象PORTのGPIOピン設定を有効化する
 */
void uart_pin_init_port1(void)
{
    GPIO_InitTypeDef gpio;

    /* Peripheral clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* USART1 GPIO Configuration. */
    gpio.Pin       = (UART_PORT1_TX_PIN | UART_PORT1_RX_PIN);
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART1;

    /* gpio port init. */
    HAL_GPIO_Init(GPIOA, &gpio);

    /* interrupt init. */
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/***
 * @name uart_pin_init_port2
 * @brief 対象PORTのGPIOピン設定を有効化する
 */
void uart_pin_init_port2(void)
{
    GPIO_InitTypeDef gpio;

    /* Peripheral clock enable */
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* USART3 TX GPIO Configuration. */
    gpio.Pin       = UART_PORT2_TX_PIN;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART3;

    HAL_GPIO_Init(GPIOB, &gpio);

    /* USART3 RX GPIO Configuration. */
    gpio.Pin       = UART_PORT2_RX_PIN;

    HAL_GPIO_Init(GPIOC, &gpio);

    /* interrupt init. */
    HAL_NVIC_SetPriority(USART3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
}

/***
 * @name uart_pin_deinit_port0
 * @brief 対象PORTのGPIOピン設定を無効化する
 */
void uart_pin_deinit_port0(void)
{
    /* Peripheral clock disable */
    __HAL_RCC_USART2_CLK_DISABLE();

    HAL_GPIO_DeInit(GPIOA, (UART_PORT0_TX_PIN | UART_PORT0_RX_PIN));
}

/***
 * @name uart_pin_deinit_port1
 * @brief 対象PORTのGPIOピン設定を無効化する
 */
void uart_pin_deinit_port1(void)
{
    /* Peripheral clock disable */
    __HAL_RCC_USART1_CLK_DISABLE();

    HAL_GPIO_DeInit(GPIOA, (UART_PORT1_TX_PIN | UART_PORT1_RX_PIN));
}

/***
 * @name uart_pin_deinit_port2
 * @brief 対象PORTのGPIOピン設定を無効化する
 */
void uart_pin_deinit_port2(void)
{
    /* Peripheral clock disable */
    __HAL_RCC_USART3_CLK_DISABLE();

    HAL_GPIO_DeInit(GPIOB, UART_PORT2_TX_PIN);
    HAL_GPIO_DeInit(GPIOC, UART_PORT2_RX_PIN);
}
