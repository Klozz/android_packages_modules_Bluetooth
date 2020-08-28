/******************************************************************************
 *
 *  Copyright 2014 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/******************************************************************************
 *
 *  This is the implementation of the API for SDP search subsystem
 *
 ******************************************************************************/

#include <string.h>

#include "bt_common.h"
#include "bta_api.h"
#include "bta_sdp_api.h"
#include "bta_sdp_int.h"
#include "bta_sys.h"
#include "port_api.h"
#include "sdp_api.h"
#include "stack/include/btu.h"

/*****************************************************************************
 *  Constants
 ****************************************************************************/

/*******************************************************************************
 *
 * Function         BTA_SdpEnable
 *
 * Description      Enable the SDP search I/F service. When the enable
 *                  operation is complete the callback function will be
 *                  called with a BTA_SDP_ENABLE_EVT. This function must
 *                  be called before other functions in the SDP search API are
 *                  called.
 *
 * Returns          BTA_SDP_SUCCESS if successful.
 *                  BTA_SDP_FAIL if internal failure.
 *
 ******************************************************************************/
tBTA_SDP_STATUS BTA_SdpEnable(tBTA_SDP_DM_CBACK* p_cback) {
  tBTA_SDP_STATUS status = BTA_SDP_FAILURE;

  APPL_TRACE_API(__func__);
  if (p_cback) {
    memset(&bta_sdp_cb, 0, sizeof(tBTA_SDP_CB));

    if (p_cback) {
      tBTA_SDP_API_ENABLE* p_buf =
          (tBTA_SDP_API_ENABLE*)osi_malloc(sizeof(tBTA_SDP_API_ENABLE));
      p_buf->hdr.event = BTA_SDP_API_ENABLE_EVT;
      p_buf->p_cback = p_cback;
      do_in_main_thread(FROM_HERE, base::Bind(bta_sdp_enable, p_buf));
      status = BTA_SDP_SUCCESS;
    }
  }
  return status;
}

/*******************************************************************************
 *
 * Function         BTA_SdpSearch
 *
 * Description      This function performs service discovery for a specific
 *                  service on given peer device. When the operation is
 *                  completed the tBTA_SDP_DM_CBACK callback function will be
 *                  called with a BTA_SDP_SEARCH_COMPLETE_EVT.
 *
 * Returns          BTA_SDP_SUCCESS, if the request is being processed.
 *                  BTA_SDP_FAILURE, otherwise.
 *
 ******************************************************************************/
tBTA_SDP_STATUS BTA_SdpSearch(const RawAddress& bd_addr,
                              const bluetooth::Uuid& uuid) {
  tBTA_SDP_API_SEARCH* p_msg =
      (tBTA_SDP_API_SEARCH*)osi_malloc(sizeof(tBTA_SDP_API_SEARCH));

  APPL_TRACE_API("%s", __func__);

  p_msg->hdr.event = BTA_SDP_API_SEARCH_EVT;
  p_msg->bd_addr = bd_addr;
  p_msg->uuid = uuid;

  do_in_main_thread(FROM_HERE, base::Bind(bta_sdp_search, p_msg));

  return BTA_SDP_SUCCESS;
}

/*******************************************************************************
 *
 * Function         BTA_SdpCreateRecordByUser
 *
 * Description      This function is used to request a callback to create a SDP
 *                  record. The registered callback will be called with event
 *                  BTA_SDP_CREATE_RECORD_USER_EVT.
 *
 * Returns          BTA_SDP_SUCCESS, if the request is being processed.
 *                  BTA_SDP_FAILURE, otherwise.
 *
 ******************************************************************************/
tBTA_SDP_STATUS BTA_SdpCreateRecordByUser(void* user_data) {
  tBTA_SDP_API_RECORD_USER* p_msg =
      (tBTA_SDP_API_RECORD_USER*)osi_malloc(sizeof(tBTA_SDP_API_RECORD_USER));

  APPL_TRACE_API("%s", __func__);

  p_msg->hdr.event = BTA_SDP_API_CREATE_RECORD_USER_EVT;
  p_msg->user_data = user_data;

  do_in_main_thread(FROM_HERE, base::Bind(bta_sdp_create_record, p_msg));

  return BTA_SDP_SUCCESS;
}

/*******************************************************************************
 *
 * Function         BTA_SdpRemoveRecordByUser
 *
 * Description      This function is used to request a callback to remove a SDP
 *                  record. The registered callback will be called with event
 *                  BTA_SDP_REMOVE_RECORD_USER_EVT.
 *
 * Returns          BTA_SDP_SUCCESS, if the request is being processed.
 *                  BTA_SDP_FAILURE, otherwise.
 *
 ******************************************************************************/
tBTA_SDP_STATUS BTA_SdpRemoveRecordByUser(void* user_data) {
  tBTA_SDP_API_RECORD_USER* p_msg =
      (tBTA_SDP_API_RECORD_USER*)osi_malloc(sizeof(tBTA_SDP_API_RECORD_USER));

  APPL_TRACE_API("%s", __func__);

  p_msg->hdr.event = BTA_SDP_API_REMOVE_RECORD_USER_EVT;
  p_msg->user_data = user_data;

  do_in_main_thread(FROM_HERE, base::Bind(bta_sdp_remove_record, p_msg));

  return BTA_SDP_SUCCESS;
}
