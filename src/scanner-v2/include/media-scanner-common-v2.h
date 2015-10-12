/*
 *  Media Server
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Yong Yeon Kim <yy9875.kim@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef _MEDIA_SCANNER_COMMON_V2_H_
#define _MEDIA_SCANNER_COMMON_V2_H_

#include "stdbool.h"
#include "media-common-system.h"

int msc_set_mmc_status(ms_stg_status_e status);
int msc_get_mmc_status(ms_stg_status_e *status);
int msc_set_power_status(bool status);
int msc_get_power_status(bool *status);

#endif /*_MEDIA_SCANNER_COMMON_V2_H_*/
