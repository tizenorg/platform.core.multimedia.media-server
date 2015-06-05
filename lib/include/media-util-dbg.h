/*
 *  Media Utility
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
 * @file		media-util-dbg.h
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */
#ifndef _MEDIA_UTIL_DBG_H_
#define _MEDIA_UTIL_DBG_H_

#include <stdio.h>
#include <stdlib.h>
#include <dlog.h>
#include <error.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "MEDIA_UTIL"
#define BUF_LENGTH 256

#define FONT_COLOR_RESET    "\033[0m"
#define FONT_COLOR_RED      "\033[31m"
#define FONT_COLOR_GREEN    "\033[32m"
#define FONT_COLOR_YELLOW   "\033[33m"
#define FONT_COLOR_BLUE     "\033[34m"
#define FONT_COLOR_PURPLE   "\033[35m"
#define FONT_COLOR_CYAN     "\033[36m"
#define FONT_COLOR_GRAY     "\033[37m"

#define MSAPI_DBG_STRERROR(fmt) do { \
			char buf[BUF_LENGTH] = {0,}; \
			strerror_r(errno, buf, BUF_LENGTH);	\
			LOGE(fmt" : STANDARD ERROR [%s]", buf);	\
		} while (0)

#define MSAPI_DBG_SLOG(fmt, args...) do { \
			SECURE_LOGD(fmt "\n", ##args);     \
		} while (0)

#define MSAPI_DBG(fmt, arg...) do { \
			LOGD(FONT_COLOR_RESET fmt, ##arg);     \
		} while (0)

#define MSAPI_DBG_INFO(fmt, arg...) do { \
			LOGI(FONT_COLOR_GREEN fmt, ##arg);     \
		} while (0)

#define MSAPI_DBG_ERR(fmt, arg...) do { \
			LOGE(FONT_COLOR_RED fmt, ##arg);     \
		} while (0)

#define MSAPI_DBG_FUNC() do { \
			LOGD(FONT_COLOR_RESET);     \
		} while (0)

#define MSAPI_RETV_IF(expr, val) do { \
			if(expr) { \
				LOGE(FONT_COLOR_RED);	  \
				return (val); \
			} \
		} while (0)

#define MSAPI_RETVM_IF(expr, val, fmt, arg...) do { \
			if(expr) { \
				LOGE(FONT_COLOR_RED fmt, ##arg);	\
				return (val); \
			} \
		} while (0)

#endif /*_MEDIA_UTIL_DBG_H_*/
