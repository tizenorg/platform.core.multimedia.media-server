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

/**
 * This file defines api utilities of contents manager engines.
 *
 * @file		media-scanner-dbg.h
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */

#ifndef _MEDIA_SCANNER_DBG_H_
#define _MEDIA_SCANNER_DBG_H_
#include <sys/syscall.h>
#include <dlog.h>
#include <errno.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "MEDIA_SCANNER"
#define BUF_LENGTH 256

#define MSC_DBG_STRERROR(fmt) do { \
			char buf[BUF_LENGTH] = {0,}; \
			strerror_r(errno, buf, BUF_LENGTH); \
			LOGE(fmt" : STANDARD ERROR [%s]", buf);	 \
		} while (0)

#define MSC_DBG_SLOG(fmt, args...)	do{ if (true) { \
		SECURE_LOGD(fmt "\n", ##args); \
		}} while(false)

#define MSC_DBG(fmt, args...) do{ if (true) { \
		LOGD(fmt "\n", ##args); \
		}} while(false)

#define MSC_DBG_INFO(fmt, args...) do{ if (true) { \
		LOGI(fmt "\n", ##args); \
		}} while(false)

#define MSC_DBG_ERR(fmt, args...) do{ if (true) { \
		LOGE(fmt "\n", ##args); \
		}} while(false)

#define MSC_DBG_WAN(fmt, args...) do{ if (true) { \
		LOGW(fmt "\n", ##args); \
		}} while(false)

/**
 * @}
 */
#endif /*_MEDIA_SCANNER_DBG_H_*/

