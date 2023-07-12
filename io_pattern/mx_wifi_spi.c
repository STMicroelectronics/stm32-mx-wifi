/**
  ******************************************************************************
  * @file    mx_wifi_spi.c
  * @author  MCD Application Team
  * @brief   This file implements the IO operations to deal with the mx_wifi
  *          module. It mainly Init and Deinit the SPI interface. Send and
  *          receive data over it.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */


/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "mx_wifi.h"
#include "mx_wifi_conf.h" /* Get some platform definitions. */
#include "mx_wifi_io.h"
#include "core/mx_wifi_hci.h"

#if (MX_WIFI_USE_SPI == 1)

#ifdef MX_WIFI_IO_DEBUG
#define DEBUG_LOG(...)       printf(__VA_ARGS__) /*;*/
#define DEBUG_WARNING(...)   printf(__VA_ARGS__) /*;*/
#else
#define DEBUG_LOG(...)
#define DEBUG_WARNING(...)
#endif /* MX_WIFI_IO_DEBUG */

#define DEBUG_ERROR(...)     printf(__VA_ARGS__) /*;*/


#pragma pack(1)
typedef struct _spi_header
{
  uint8_t  type;
  uint16_t len;
  uint16_t lenx;
  uint8_t  dummy[3];
} spi_header_t;
#pragma pack()


#ifndef MX_WIFI_RESET_PIN

#define MX_WIFI_RESET_PIN        MXCHIP_RESET_Pin
#define MX_WIFI_RESET_PORT       MXCHIP_RESET_GPIO_Port

/* wifi spi cs */
#define MX_WIFI_SPI_CS_PIN       MXCHIP_NSS_Pin
#define MX_WIFI_SPI_CS_PORT      MXCHIP_NSS_GPIO_Port

/* wifi spi slave notify */
#define MX_WIFI_SPI_IRQ_PIN      MXCHIP_NOTIFY_Pin
#define MX_WIFI_SPI_IRQ_PORT     MXCHIP_NOTIFY_GPIO_Port
#define MX_WIFI_SPI_IRQ          MXCHIP_NOTIFY_EXTI_IRQn

#define MX_WIFI_SPI_FLOW_PIN     MXCHIP_FLOW_Pin
#define MX_WIFI_SPI_FLOW_PORT    MXCHIP_FLOW_GPIO_Port

#endif /* MX_WIFI_RESET_PIN */


#ifndef NET_PERF_TASK_TAG
#define NET_PERF_TASK_TAG(...)
#endif /* NET_PERF_TASK_TAG */

/* Private define ------------------------------------------------------------*/
/* SPI protocol */
#define SPI_WRITE         ((uint8_t)0x0A)
#define SPI_READ          ((uint8_t)0x0B)
#define SPI_DATA_SIZE     (MX_WIFI_HCI_DATA_SIZE)

/* HW RESET */

#define MX_WIFI_HW_RESET()                                                    \
  do {                                                                        \
    HAL_GPIO_WritePin(MX_WIFI_RESET_PORT, MX_WIFI_RESET_PIN, GPIO_PIN_RESET); \
    HAL_Delay(100);                                                           \
    HAL_GPIO_WritePin(MX_WIFI_RESET_PORT, MX_WIFI_RESET_PIN, GPIO_PIN_SET);   \
    HAL_Delay(1200);                                                          \
    DEBUG_LOG("\n[%" PRIu32 "] MX_WIFI_HW_RESET\n\n", HAL_GetTick());         \
  } while(0)

/* SPI CS */
#define MX_WIFI_SPI_CS_HIGH()                                                 \
  do {                                                                        \
    HAL_GPIO_WritePin(MX_WIFI_SPI_CS_PORT, MX_WIFI_SPI_CS_PIN, GPIO_PIN_SET); \
  } while(0)


#define MX_WIFI_SPI_CS_LOW()                                                   \
  do {                                                                         \
    HAL_GPIO_WritePin(MX_WIFI_SPI_CS_PORT, MX_WIFI_SPI_CS_PIN, GPIO_PIN_RESET);\
  } while(0)

/* SPI IRQ */
#define MX_WIFI_SPI_IRQ_IS_HIGH() \
  (GPIO_PIN_SET == HAL_GPIO_ReadPin(MX_WIFI_SPI_IRQ_PORT, MX_WIFI_SPI_IRQ_PIN))

#define MX_WIFI_SPI_FLOW_IS_LOW() \
  (GPIO_PIN_RESET == HAL_GPIO_ReadPin(MX_WIFI_SPI_FLOW_PORT, MX_WIFI_SPI_FLOW_PIN))

/* Global variables  ---------------------------------------------------------*/
extern SPI_HandleTypeDef MXCHIP_SPI;

/* Private variables ---------------------------------------------------------*/
static MX_WIFIObject_t MxWifiObj;
static SPI_HandleTypeDef *const HSpiMX = &MXCHIP_SPI;

static LOCK_DECLARE(SpiTxLock);

static SEM_DECLARE(SpiTxRxSem);
static SEM_DECLARE(SpiFlowRiseSem);
static SEM_DECLARE(SpiTransferDoneSem);

static uint8_t *SpiTxData = NULL;
static uint16_t SpiTxLen  = 0;

THREAD_DECLARE(MX_WIFI_TxRxThreadId);


/* Private functions ---------------------------------------------------------*/
static uint16_t MX_WIFI_SPI_Read(uint8_t *buffer, uint16_t buff_size);
static HAL_StatusTypeDef TransmitReceive(SPI_HandleTypeDef *hspi, uint8_t *txdata, uint8_t *rxdata, uint32_t datalen,
                                         uint32_t timeout);
static HAL_StatusTypeDef Transmit(SPI_HandleTypeDef *hspi, uint8_t *txdata, uint32_t datalen, uint32_t timeout);
static HAL_StatusTypeDef Receive(SPI_HandleTypeDef *hspi, uint8_t *rxdata, uint32_t datalen, uint32_t timeout);

static int8_t wait_flow_high(uint32_t timeout);
static uint16_t MX_WIFI_SPI_Write(uint8_t *data, uint16_t len);

static int8_t mx_wifi_spi_txrx_start(void);
static int8_t mx_wifi_spi_txrx_stop(void);

static void MX_WIFI_IO_DELAY(uint32_t ms);
static int8_t MX_WIFI_SPI_Init(uint16_t mode);
static int8_t MX_WIFI_SPI_DeInit(void);


#ifndef MX_WIFI_BARE_OS_H
static __IO bool SPITxRxTaskQuitFlag;

static void mx_wifi_spi_txrx_task(THREAD_CONTEXT_TYPE argument);
#endif /* MX_WIFI_BARE_OS_H */


static void MX_WIFI_IO_DELAY(uint32_t ms)
{
  DELAY_MS(ms);
}


/**
  * @brief  Initialize the SPI
  * @param  mode
  * @retval status
  */
static int8_t MX_WIFI_SPI_Init(uint16_t mode)
{
  int8_t ret = 0;

  if (MX_WIFI_RESET == mode)
  {
    MX_WIFI_HW_RESET();
  }
  else
  {
    ret = mx_wifi_spi_txrx_start();
  }
  return ret;
}


/**
  * @brief  De-Initialize the SPI
  * @param  None
  * @retval status
  */
static int8_t MX_WIFI_SPI_DeInit(void)
{
  mx_wifi_spi_txrx_stop();
  return 0;
}


void HAL_SPI_TransferCallback(void *hspi)
{
  SEM_SIGNAL(SpiTransferDoneSem);
}


void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
  while (1);
}


/**
  * @brief  Interrupt handler for IRQ and FLOW pin
  * @param  isr_source
  * @retval None
  */
void mxchip_WIFI_ISR(uint16_t isr_source)
{
  /*DEBUG_LOG("\n[%"PRIu32"] %s()> %" PRIx32 "\n\n", HAL_GetTick(), __FUNCTION__, (uint32_t)isr_source);*/

  if (MX_WIFI_SPI_IRQ_PIN == isr_source)
  {
    SEM_SIGNAL(SpiTxRxSem);
  }
  if (MX_WIFI_SPI_FLOW_PIN == isr_source)
  {
    SEM_SIGNAL(SpiFlowRiseSem);
  }
}


static int8_t wait_flow_high(uint32_t timeout)
{
  int8_t ret = 0;
  if (SEM_WAIT(SpiFlowRiseSem, timeout, NULL) != SEM_OK)
  {
    ret = -1;
  }
  if (MX_WIFI_SPI_FLOW_IS_LOW())
  {
    DEBUG_ERROR("FLOW is low\n");
    ret = -1;
  }

  DEBUG_LOG("\n%s()< %" PRIi32 "\n\n", __FUNCTION__, (int32_t)ret);

  return ret;
}


static uint16_t MX_WIFI_SPI_Write(uint8_t *data, uint16_t len)
{
  uint16_t sent;

  LOCK(SpiTxLock);

  if ((NULL == data) || (0 == len) || (len > SPI_DATA_SIZE))
  {
    DEBUG_ERROR("Warning, spi send null or size overflow! len=%" PRIu32 "\n", (uint32_t)len);
    SpiTxLen = 0;
    sent = 0;
  }
  else
  {
    SpiTxData = data;
    SpiTxLen  = len;

    if (SEM_SIGNAL(SpiTxRxSem) != SEM_OK)
    {
      /* Happen if received thread did not have a chance to run on time, need to increase priority */
      DEBUG_ERROR("Warning, spi semaphore has been already notified\n");
    }
    sent = len;
  }
  DEBUG_LOG("\n%s()< %" PRIi32 "\n\n", __FUNCTION__, (int32_t)sent);

  UNLOCK(SpiTxLock);

  return sent;
}


static uint16_t MX_WIFI_SPI_Read(uint8_t *buffer, uint16_t buff_size)
{
  return 0;
}


#if (DMA_ON_USE == 1)

static HAL_StatusTypeDef TransmitReceive(SPI_HandleTypeDef *hspi, uint8_t *txdata, uint8_t *rxdata, uint32_t datalen,
                                         uint32_t timeout)
{
  HAL_StatusTypeDef ret;

  DEBUG_LOG(("\n%s()> %"PRIu32"\n"), __FUNCTION__, datalen);

#if 0
  for (uint32_t i = 0; i < datalen; i++)
  {
    DEBUG_LOG("%02" PRIx32 " ", (uint32_t)txdata[i]);
  }
#endif /* 0 */

  ret = HAL_SPI_TransmitReceive_DMA(hspi, txdata, rxdata, datalen);
  SEM_WAIT(SpiTransferDoneSem, timeout, NULL);

  DEBUG_LOG("\n%s()< %" PRIi32 "\n\n", __FUNCTION__, (int32_t)ret);

  return ret;
}


static HAL_StatusTypeDef Transmit(SPI_HandleTypeDef *hspi, uint8_t *txdata, uint32_t datalen, uint32_t timeout)
{
  HAL_StatusTypeDef ret;

  DEBUG_LOG("\n%s()> %" PRIu32 "\n", __FUNCTION__, datalen);

  ret = HAL_SPI_Transmit_DMA(hspi, txdata, datalen);
  SEM_WAIT(SpiTransferDoneSem, timeout, NULL);

#if 0
  for (uint32_t i = 0; i < datalen; i++)
  {
    DEBUG_LOG("%02" PRIx32 " ", (uint32_t)txdata[i]);
  }
#endif /* 0 */

  DEBUG_LOG("\n%s() <%" PRIi32 "\n\n", __FUNCTION__, (int32_t)ret);

  return ret;
}


static HAL_StatusTypeDef Receive(SPI_HandleTypeDef *hspi, uint8_t *rxdata, uint32_t datalen, uint32_t timeout)
{
  HAL_StatusTypeDef ret;

  DEBUG_LOG("\n%s()> %" PRIu32 "\n", __FUNCTION__, datalen);

  ret = HAL_SPI_Receive_DMA(hspi, rxdata, datalen);
  SEM_WAIT(SpiTransferDoneSem, timeout, NULL);

#if 0
  for (uint32_t i = 0; i < datalen; i++)
  {
    DEBUG_LOG("%02" PRIx32 " ", (uint32_t)rxdata[i]);
  }
#endif /* 0 */

  DEBUG_LOG("\n%s()< %" PRIi32 "\n\n", __FUNCTION__, (int32_t)ret);

  return ret;
}

#else

static HAL_StatusTypeDef TransmitReceive(SPI_HandleTypeDef *hspi, uint8_t *txdata, uint8_t *rxdata, uint32_t datalen,
                                         uint32_t timeout)
{
  HAL_StatusTypeDef ret;
  DEBUG_LOG("Spi Tx Rx %d\n", datalen);
  ret = HAL_SPI_TransmitReceive(hspi, txdata, rxdata, datalen, timeout);

  DEBUG_LOG("\n%s()< %" PRIi32 "\n\n", __FUNCTION__, (int32_t)ret);

  return ret;
}


static HAL_StatusTypeDef Transmit(SPI_HandleTypeDef *hspi, uint8_t *txdata, uint32_t datalen, uint32_t timeout)
{
  HAL_StatusTypeDef ret;
  DEBUG_LOG("Spi Tx %d\n", datalen);
  ret = HAL_SPI_Transmit(hspi, txdata, datalen, timeout);

  DEBUG_LOG("\n%s()< %" PRIi32 "\n\n", __FUNCTION__, (int32_t)ret);

  return ret;
}


static HAL_StatusTypeDef Receive(SPI_HandleTypeDef *hspi, uint8_t *rxdata, uint32_t datalen, uint32_t timeout)
{
  HAL_StatusTypeDef ret;
  DEBUG_LOG("Spi Rx %d\n", datalen);
  ret = HAL_SPI_Receive(hspi, rxdata, datalen, timeout);

  DEBUG_LOG("\n%s()< %" PRIi32 "\n\n", __FUNCTION__, (int32_t)ret);

  return ret;
}
#endif /* (DMA_ON_USE == 1) */


void process_txrx_poll(uint32_t timeout)
{
  static mx_buf_t *netb = NULL;
  bool first_miss = true;

  MX_WIFI_SPI_CS_HIGH();

  while (netb == NULL)
  {
    netb = MX_NET_BUFFER_ALLOC(MX_WIFI_BUFFER_SIZE);
    if (netb == NULL)
    {
      DELAY_MS(1);
      if (true == first_miss)
      {
        first_miss = false;
        DEBUG_WARNING("Running Out of buffer for RX\n");
      }
    }
  }

  /* Waiting for data to be sent or to be received. */
  if (SEM_WAIT(SpiTxRxSem, timeout, NULL) == SEM_OK)
  {
    spi_header_t mheader = {0};
    spi_header_t sheader = {0};
    uint8_t *txdata = NULL;
    uint8_t *rxdata = NULL;
    uint16_t datalen;
    HAL_StatusTypeDef ret;

    NET_PERF_TASK_TAG(0);

    DEBUG_LOG("\n%s(): %p\n", __FUNCTION__, SpiTxData);

    if (SpiTxData == NULL)
    {
      if (!MX_WIFI_SPI_IRQ_IS_HIGH())
      {
        /* TX data null means no data to send, IRQ low means no data to be received. */
        MX_NET_BUFFER_FREE(netb);
        return;
      }
    }
    else
    {
      mheader.len = SpiTxLen;
      txdata = SpiTxData;
    }

    mheader.type = SPI_WRITE;
    mheader.lenx = ~mheader.len;

    MX_WIFI_SPI_CS_LOW();

    /* Wait for the EMW to be ready. */
    if (wait_flow_high(20) != 0)
    {
      MX_WIFI_SPI_CS_HIGH();
      DEBUG_ERROR("Wait FLOW timeout 0\n");
      return;
    }

    /* Transmit only header part. */
    if (HAL_OK != TransmitReceive(HSpiMX, (uint8_t *)&mheader, (uint8_t *)&sheader, sizeof(mheader), timeout))
    {
      MX_WIFI_SPI_CS_HIGH();
      DEBUG_ERROR("Send mheader error\n");
      return;
    }

    if (sheader.type != SPI_READ)
    {
      DEBUG_ERROR("Invalid SPI type %02x\n", sheader.type);
      MX_WIFI_SPI_CS_HIGH();
      return;
    }

    if ((sheader.len ^ sheader.lenx) != 0xFFFF)
    {
      MX_WIFI_SPI_CS_HIGH();
      DEBUG_ERROR("Invalid len %04x-%04x\n", sheader.len, sheader.lenx);
      return;
    }

    /* send or received header must be not null */
    if ((sheader.len == 0) && (mheader.len == 0))
    {
      MX_WIFI_SPI_CS_HIGH();
      return;
    }

    if ((sheader.len > SPI_DATA_SIZE) || (mheader.len > SPI_DATA_SIZE))
    {
      MX_WIFI_SPI_CS_HIGH();
      DEBUG_ERROR("SPI length invalid: %d-%d\n", sheader.len, mheader.len);
      return;
    }

    /* Keep the max length between TX and RX. */
    if (mheader.len > sheader.len)
    {
      datalen = mheader.len;
    }
    else
    {
      datalen = sheader.len;
    }

    /* Allocate a buffer for data to be received. */
    if (sheader.len > 0)
    {
      /* Get start of the buffer payload. */
      rxdata = MX_NET_BUFFER_PAYLOAD(netb);
    }

    /* FLOW must be high. */
    if (wait_flow_high(20) != 0)
    {
      MX_WIFI_SPI_CS_HIGH();
      DEBUG_ERROR("Wait FLOW timeout 1\n");
      return;
    }

    /* TX with possible RX. */
    if (NULL != txdata)
    {
      SpiTxData = NULL;
      SpiTxLen = 0;
      if (NULL != rxdata)
      {
        ret = TransmitReceive(HSpiMX, txdata, rxdata, datalen, timeout);
      }
      else
      {
        ret = Transmit(HSpiMX, txdata, datalen, timeout);
      }
    }
    else
    {
      ret = Receive(HSpiMX, rxdata, datalen, timeout);
    }

    if (HAL_OK != ret)
    {
      MX_WIFI_SPI_CS_HIGH();
      DEBUG_ERROR("Transmit/Receive data timeout\n");
      return;
    }

    /* Resize the input buffer and send it back to the processing thread. */
    if (sheader.len > 0)
    {
      NET_PERF_TASK_TAG(1);
      MX_NET_BUFFER_SET_PAYLOAD_SIZE(netb, sheader.len);
      mx_wifi_hci_input(netb);
      netb = NULL;
    }
    else
    {
      NET_PERF_TASK_TAG(2);
    }
  }
}


#ifndef MX_WIFI_BARE_OS_H
static void mx_wifi_spi_txrx_task(THREAD_CONTEXT_TYPE argument)
{

  SPITxRxTaskQuitFlag = false;

  while (SPITxRxTaskQuitFlag != true)
  {
    process_txrx_poll(WAIT_FOREVER);
  }

  SPITxRxTaskQuitFlag = false;

  /* Prepare deletion (depends on implementation). */
  THREAD_TERMINATE();

  /* Delete the Thread. */
  THREAD_DEINIT(MX_WIFI_TxRxThreadId);
}
#endif /* MX_WIFI_BARE_OS_H */


static int8_t mx_wifi_spi_txrx_start(void)
{
  int8_t ret = 0;

  LOCK_INIT(SpiTxLock);
  SEM_INIT(SpiTxRxSem, 2);
  SEM_INIT(SpiFlowRiseSem, 1);
  SEM_INIT(SpiTransferDoneSem, 1);


  if (THREAD_OK != THREAD_INIT(MX_WIFI_TxRxThreadId, mx_wifi_spi_txrx_task, NULL,
                               MX_WIFI_SPI_THREAD_STACK_SIZE,
                               MX_WIFI_SPI_THREAD_PRIORITY))
  {
    ret = -1;
  }
  else
  {
    /* De-select the SPI slave. */
    MX_WIFI_SPI_CS_HIGH();
  }
  return ret;
}


static int8_t mx_wifi_spi_txrx_stop(void)
{
#ifndef MX_WIFI_BARE_OS_H
  /* Set thread quit flag to TRUE. */
  SPITxRxTaskQuitFlag = true;
#endif /* MX_WIFI_BARE_OS_H */

  /* Wake up the thread if it's sleeping. */
  SEM_SIGNAL(SpiTxRxSem);

#ifndef MX_WIFI_BARE_OS_H
  /* Wait for the thread to terminate. */
  while (SPITxRxTaskQuitFlag == true)
  {
    DELAY_MS(500);
  }
#endif /* MX_WIFI_BARE_OS_H */

  /* Delete the Thread (depends on implementation). */
  THREAD_DEINIT(MX_WIFI_TxRxThreadId);
  SEM_DEINIT(SpiTxRxSem);
  SEM_DEINIT(SpiFlowRiseSem);
  LOCK_DEINIT(SpiTxLock);
  return 0;
}


int32_t mxwifi_probe(void **ll_drv_context)
{
  int32_t ret;
  if (MX_WIFI_RegisterBusIO(&MxWifiObj,
                            MX_WIFI_SPI_Init,
                            MX_WIFI_SPI_DeInit,
                            MX_WIFI_IO_DELAY,
                            MX_WIFI_SPI_Write,
                            MX_WIFI_SPI_Read) == MX_WIFI_STATUS_OK)
  {
    *ll_drv_context = &MxWifiObj;
    ret = 0;
  }
  else
  {
    ret = -1;
  }

  return ret;
}


MX_WIFIObject_t *wifi_obj_get(void)
{
  return &MxWifiObj;
}

#endif /* (MX_WIFI_USE_SPI == 1) */
