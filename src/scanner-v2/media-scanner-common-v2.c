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
#include "media-util-err.h"

#include "media-scanner-dbg-v2.h"
#include "media-scanner-common-v2.h"

int mmc_state2 = 0;
bool power_off2;

int msc_set_mmc_status(ms_stg_status_e status)
{
	mmc_state2 = status;

	return MS_MEDIA_ERR_NONE;
}

int msc_get_mmc_status(ms_stg_status_e *status)
{
	*status = mmc_state2;

	return MS_MEDIA_ERR_NONE;
}

int msc_set_power_status(bool status)
{
	power_off2 = status;

	return MS_MEDIA_ERR_NONE;
}

int msc_get_power_status(bool *status)
{
	*status = power_off2;

	return MS_MEDIA_ERR_NONE;
}

