/*
 *  Testcase for libmedia-utils
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

#include <tet_api.h>
#include <stdio.h>
#include <string.h>
#include <media-util-register.h>
/****************************************************
* add your header file
* e.g., 
* #include <time.h>
* #include <stdio.h>
* #include <stdlib.h>
* #include <getopt.h>
* #include "calendar.h"
****************************************************/

static void startup(), cleanup();
void (*tet_startup) () = startup;
void (*tet_cleanup) () = cleanup;

/****************************************************
* add your tc function 
* e.g.,
* static void utc_ApplicationLib_recurGetDayOfWeek_func_01(void);
* static void utc_ApplicationLib_recurGetDayOfWeek_func_02(void);
****************************************************/

static void utc_media_file_register_01();
static void utc_media_file_register_02();

/****************************************************
* add your test case list here
* e.g., 
* struct tet_testlist tet_testlist[] =	{
*			{ utc_ApplicationLib_recurGetDayOfWeek_func_01,1},
*			{ utc_ApplicationLib_recurGetDayOfWeek_func_02,2},
*			{NULL,0}
* };
****************************************************/

struct tet_testlist tet_testlist[] = {
	{utc_media_file_register_01, 1},
	{utc_media_file_register_02, 2},
	{NULL, 0}
};

