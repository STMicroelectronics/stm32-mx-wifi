/**
  ******************************************************************************
  * @file    mx_wifi_ipc.c
  * @author  MCD Application Team
  * @brief   Host driver IPC protocol of MXCHIP Wi-Fi component.
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
#include "mx_wifi_ipc.h"
#include "mx_wifi_hci.h"
#include "io_pattern/mx_wifi_io.h"


#ifdef MX_WIFI_IPC_DEBUG
#define DEBUG_LOG(...)       printf(__VA_ARGS__) /*;*/
#else
#define DEBUG_LOG(M, ...)
#endif /* MX_WIFI_IPC_DEBUG */

#define DEBUG_ERROR(...)     printf(__VA_ARGS__) /*;*/


/**
  * @brief IPC API event handlers
  */
typedef void (*event_callback_t)(mx_buf_t *mx_buff);

typedef struct _event_item_s
{
  uint16_t api_id;
  event_callback_t callback;
} event_item_t;


static const event_item_t event_table[] =
{
  /* System */
  {MIPC_API_SYS_REBOOT_EVENT,         mapi_reboot_event_callback},
  /* wifi */
  {MIPC_API_WIFI_STATUS_EVENT,        mapi_wifi_status_event_callback},
  {MIPC_API_WIFI_BYPASS_INPUT_EVENT,  mapi_wifi_netlink_input_callback}
};


/**
  * @brief IPC API request list
  */
typedef struct _mipc_req_s
{
  uint32_t req_id;
  SEM_DECLARE(resp_flag);
  uint16_t *rbuffer_size; /* in/out */
  uint8_t *rbuffer;
} mipc_req_t;

#define MIPC_REQ_ID_RESET_VAL  ((uint32_t)(0xFFFFFFFF))


static mipc_req_t PendingRequest;

static uint32_t get_new_req_id(void);
static uint32_t mpic_get_req_id(const uint8_t *buffer_in);
static uint16_t mpic_get_api_id(const uint8_t *buffer_in);
static void mipc_event(mx_buf_t *netbuf);

/* unique sequence number */
static uint32_t get_new_req_id(void)
{
  static uint32_t id = 1;
  return id++;
}


static uint32_t mpic_get_req_id(const uint8_t *buffer_in)
{
  return *((uint32_t *) & (buffer_in[MIPC_PKT_REQ_ID_OFFSET]));
}


static uint16_t mpic_get_api_id(const uint8_t *buffer_in)
{
  return *((uint16_t *) & (buffer_in[MIPC_PKT_API_ID_OFFSET]));
}


static void mipc_event(mx_buf_t *netbuf)
{
  if (NULL != netbuf)
  {
    uint8_t *const buffer_in = MX_NET_BUFFER_PAYLOAD(netbuf);
    const uint32_t buffer_in_size = MX_NET_BUFFER_GET_PAYLOAD_SIZE(netbuf);

    if ((NULL != buffer_in) && (buffer_in_size >= MIPC_PKT_MIN_SIZE))
    {
      const uint32_t req_id = mpic_get_req_id(buffer_in);
      const uint16_t api_id = mpic_get_api_id(buffer_in);

      DEBUG_LOG("%-15s(): req_id: 0x%08" PRIx32 ", api_id: 0x%04" PRIx32 "\n", __FUNCTION__, req_id, (uint32_t)api_id);

      if ((0 == (api_id & MIPC_API_EVENT_BASE)) && (MIPC_REQ_ID_NONE != req_id))
      {
        /* The command response must match pending req id. */
        MX_ASSERT(PendingRequest.req_id == req_id);
        {
          /* return params */
          if ((PendingRequest.rbuffer_size != NULL) && (*(PendingRequest.rbuffer_size) > 0)
              && (NULL != PendingRequest.rbuffer))
          {
            *(PendingRequest.rbuffer_size) = *(PendingRequest.rbuffer_size) < (buffer_in_size - MIPC_PKT_MIN_SIZE) ? \
                                             *(PendingRequest.rbuffer_size) : (buffer_in_size - MIPC_PKT_MIN_SIZE);
            memcpy(PendingRequest.rbuffer, &buffer_in[MIPC_PKT_PARAMS_OFFSET], *(PendingRequest.rbuffer_size));
          }
          /* printf("Signal for %d\n",pending_request.req_id); */
          PendingRequest.req_id = MIPC_REQ_ID_RESET_VAL;
          if (SEM_OK != SEM_SIGNAL(PendingRequest.resp_flag))
          {
            DEBUG_ERROR("Failed to signal command response\n");
            while (1);
          }
          MX_STAT(cmd_get_answer);
          mx_wifi_hci_free(netbuf);
        }
      }
      else /* event callback */
      {
        const uint32_t event_table_count = sizeof(event_table) / sizeof(event_table[0]);
        uint32_t i;

        for (i = 0; i < event_table_count; i++)
        {
          if (event_table[i].api_id == api_id)
          {
            const event_callback_t callback = event_table[i].callback;
            if (NULL != callback)
            {
              /*DEBUG_LOG("callback with %p\n", buffer_in);*/
              callback(netbuf);
              break;
            }
          }
        }
        if (i == event_table_count)
        {
          DEBUG_ERROR("Unknown event: 0x%04" PRIx32 "!\n", (uint32_t)api_id);
          mx_wifi_hci_free(netbuf);
        }
      }
    }
    else
    {
      DEBUG_LOG("Unknown buffer content\n");
      mx_wifi_hci_free(netbuf);
    }
  }
}

/*******************************************************************************
  * IPC API implementations for mx_wifi over HCI
  ******************************************************************************/

int32_t mipc_init(mipc_send_func_t ipc_send)
{
  int32_t ret;

  PendingRequest.req_id = MIPC_REQ_ID_RESET_VAL;
  SEM_INIT(PendingRequest.resp_flag, 1);

  ret = mx_wifi_hci_init(ipc_send);

  return ret;
}


int32_t mipc_deinit(void)
{
  int32_t ret;

  SEM_DEINIT(PendingRequest.resp_flag);

  ret = mx_wifi_hci_deinit();

  return ret;
}


int32_t mipc_request(uint16_t api_id,
                     uint8_t *cparams, uint16_t cparams_size,
                     uint8_t *rbuffer, uint16_t *rbuffer_size,
                     uint32_t timeout_ms)
{
  int32_t ret = MIPC_CODE_ERROR;
  uint8_t *cbuf;
  bool copy_buffer = true;

  LOCK(wifi_obj_get()->lockcmd);

  /* DEBUG_LOG("\n%s()>  %" PRIu32 "\n", __FUNCTION__, (uint32_t)cparams_size); */

  if (cparams_size <= MX_WIFI_IPC_PAYLOAD_SIZE)
  {
    /* Create the command data. */
    const uint16_t cbuf_size = MIPC_PKT_REQ_ID_SIZE + MIPC_PKT_API_ID_SIZE + cparams_size;

#if MX_WIFI_TX_BUFFER_NO_COPY
    if (api_id == MIPC_API_WIFI_BYPASS_OUT_CMD)
    {
      cbuf = cparams - MIPC_PKT_REQ_ID_SIZE - MIPC_PKT_API_ID_SIZE;
      copy_buffer = false;
    }
    else
#endif /* MX_WIFI_TX_BUFFER_NO_COPY */
    {
      DEBUG_LOG("\n%-15s(): Allocate %" PRIu32 " bytes\n", __FUNCTION__, (uint32_t)cbuf_size);

      cbuf = (uint8_t *)MX_WIFI_MALLOC(cbuf_size);
    }

    if (NULL != cbuf)
    {
      /* Get an unique identifier. */
      const uint32_t req_id = get_new_req_id();

      /* Copy the protocol parameter to the head part of the buffer. */
      memcpy(&cbuf[MIPC_PKT_REQ_ID_OFFSET], &req_id, sizeof(req_id));
      memcpy(&cbuf[MIPC_PKT_API_ID_OFFSET], &api_id, sizeof(api_id));

      if ((true == copy_buffer) && (cparams_size > 0))
      {
        memcpy(&cbuf[MIPC_PKT_PARAMS_OFFSET], cparams, cparams_size);
      }

      /* A single pending request due to LOCK usage on command. */
      if (PendingRequest.req_id != MIPC_REQ_ID_RESET_VAL)
      {
        DEBUG_LOG("Error req_id must be 0xffffffff here %" PRIu32 "\n", PendingRequest.req_id);
        while (1);
      }

      PendingRequest.req_id = req_id;
      PendingRequest.rbuffer = rbuffer;
      PendingRequest.rbuffer_size = rbuffer_size;

      /* static int iter=0;                       */
      /* printf("%d push %d\n",iter++,cbuf_size); */

      /* Send the command. */
      DEBUG_LOG("%-15s(): req_id: 0x%08" PRIx32 " : %" PRIu32 "\n", __FUNCTION__, req_id, (uint32_t)cbuf_size);

      ret = mx_wifi_hci_send(cbuf, cbuf_size);
      if (ret == 0)
      {
        /* Wait for the command answer. */
        if (SEM_WAIT(PendingRequest.resp_flag, timeout_ms, mipc_poll) != SEM_OK)
        {
          DEBUG_ERROR("Error: command 0x%04" PRIx32" timeout(%" PRIu32 " ms) waiting answer %" PRIu32 "\n",
                      (uint32_t)api_id, timeout_ms, PendingRequest.req_id);
          PendingRequest.req_id = MIPC_REQ_ID_RESET_VAL;
          ret = MIPC_CODE_ERROR;
        }
      }
      else
      {
        DEBUG_ERROR("Failed to send command to HCI\n");
        while (1);
      }

      DEBUG_LOG("%-15s()< req_id: 0x%08" PRIx32 " done\n\n", __FUNCTION__, req_id);

      if (true == copy_buffer)
      {
        MX_WIFI_FREE(cbuf);
      }
    }
  }

  UNLOCK(wifi_obj_get()->lockcmd);

  return ret;
}


void mipc_poll(uint32_t timeout)
{
  mx_buf_t *nbuf;

  /* process the received data inside RX buffer */
  nbuf = mx_wifi_hci_recv(timeout);

  if (NULL != nbuf)
  {
    const uint32_t len = MX_NET_BUFFER_GET_PAYLOAD_SIZE(nbuf);
    DEBUG_LOG("%-15s(): hci recv len %" PRIu32 "\n", __FUNCTION__, len);

    if (len > 0U)
    {
      mipc_event(nbuf);
    }
    else
    {
      MX_NET_BUFFER_FREE(nbuf);
    }
  }
}


int32_t mipc_echo(uint8_t *in, uint16_t in_len, uint8_t *out, uint16_t *out_len,
                  uint32_t timeout)
{
  int32_t ret = MIPC_CODE_ERROR;

  if ((NULL != in) && (NULL != out) && (NULL != out_len))
  {
    ret = mipc_request(MIPC_API_SYS_ECHO_CMD, in, in_len, out, out_len, timeout);
    if (MIPC_CODE_SUCCESS != ret)
    {
      *out_len = 0;
    }
  }
  return ret;
}

/*******************************************************************************
  * IPC API event callbacks
  ******************************************************************************/

/* system */

void mapi_reboot_event_callback(mx_buf_t *buff)
{
  if (buff != NULL)
  {
    mx_wifi_hci_free(buff);
  }
  DEBUG_LOG("\nEVENT: reboot done.\n");
}

/* wifi */

void mapi_wifi_status_event_callback(mx_buf_t *nbuf)
{
  uint8_t cate;
  mwifi_status_t status;
  mx_wifi_status_callback_t status_cb = NULL;
  void *cb_args = NULL;

  if (NULL != nbuf)
  {
    uint8_t *payload = MX_NET_BUFFER_PAYLOAD(nbuf);
    status = *((mwifi_status_t *)(payload + MIPC_PKT_PARAMS_OFFSET));
    DEBUG_LOG("\nEVENT: wifi status: %02x\r\n", status);
    mx_wifi_hci_free(nbuf);

    switch (status)
    {
      case MWIFI_EVENT_STA_UP:
      case MWIFI_EVENT_STA_DOWN:
      case MWIFI_EVENT_STA_GOT_IP:
        cate = MC_STATION;
        status_cb = wifi_obj_get()->Runtime.status_cb[0];
        cb_args = wifi_obj_get()->Runtime.callback_arg[0];
        break;

      case MWIFI_EVENT_AP_UP:
      case MWIFI_EVENT_AP_DOWN:
        cate = MC_SOFTAP;
        status_cb = wifi_obj_get()->Runtime.status_cb[0];
        cb_args = wifi_obj_get()->Runtime.callback_arg[0];
        break;

      default:
        cate = MC_SOFTAP;
        MX_ASSERT(false);
        break;
    }

    if (NULL != status_cb)
    {
      status_cb(cate, status, cb_args);
    }
  }
}


void mapi_wifi_netlink_input_callback(mx_buf_t *nbuf)
{
  if (NULL != nbuf)
  {
    uint8_t *const buffer_in = MX_NET_BUFFER_PAYLOAD(nbuf);
    wifi_bypass_in_rparams_t *const in_rprarams = (wifi_bypass_in_rparams_t *)(buffer_in + MIPC_PKT_PARAMS_OFFSET);

    MX_STAT(callback);

    if ((NULL != wifi_obj_get()->Runtime.netlink_input_cb) && \
        (in_rprarams->tot_len > 0))
    {
      MX_NET_BUFFER_HIDE_HEADER(nbuf, MIPC_PKT_PARAMS_OFFSET + sizeof(wifi_bypass_in_rparams_t));

      wifi_obj_get()->Runtime.netlink_input_cb(nbuf, wifi_obj_get()->Runtime.netlink_user_args);
    }
    else
    {
      MX_NET_BUFFER_FREE(nbuf);
    }
  }
}
