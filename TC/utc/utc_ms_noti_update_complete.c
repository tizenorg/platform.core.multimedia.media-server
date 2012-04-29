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

/***************************************
* add your header file here
* e.g.,
* #include "utc_ApplicationLib_recurGetDayOfWeek_func.h"
***************************************/
#include "utc_ms_noti_update_complete.h"

/***************************************
* add Test Case Wide Declarations
* e.g.,
* static char msg[256];
***************************************/

static void startup()
{
	tet_printf("\n TC startup");
	/* add your code here */

}

static void cleanup()
{
	tet_printf("\n TC End");
	/* add your code here */

}

/***************************************
* Add your callback function here
* if needed
***************************************/

/***************************************
* add your Test Cases
* e.g.,
* static void utc_ApplicationLib_recurGetDayOfWeek_01()
* {
* 	int ret;
*	
* 	ret = target_api();
* 	if(ret == 1)
* 		tet_result(TET_PASS);
* 	else
* 		tet_result(TET_FAIL);	
* }
*
* static void utc_ApplicationLib_recurGetDayOfWeek_02()
* {
* 		code..
*		condition1
* 			tet_result(TET_PASS);
*		condition2
*			tet_result(TET_FAIL);
* }
*
***************************************/

static void utc_ms_noti_update_complete_01()
{
	int ret = 0;

	ret = ms_noti_update_complete();
	if (ret == 0) {
		tet_result(TET_PASS);
		rmdir("/opt/data/file-manager-service/_FILEOPERATION_END");
	}
	else
		tet_result(TET_FAIL);
}

