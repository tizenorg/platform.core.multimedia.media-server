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
 * @file		media-util-noti.h
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */
 #ifndef _MEDIA_UTIL_NOTI_H_
#define _MEDIA_UTIL_NOTI_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
* @fn 		int ms_noti_db_update_complete(void);
* @brief 		This function announce media database is updated to other applications.<br>
* @return	This function returns 0 on success, and -1 on failure.
* @param[in]	none
* @remark  	This function is recommandation for other application being aware of database updating.<br>
* @par example
* @code

#include <stdio.h>
#include <string.h>
#include <media-service-noti.h>

	int main()
	{
		int result;

		....
		file operation (copy/move/delete)
		....

		result = ms_noti_db_update_complete();
		if( result < 0 )
		{
			printf("FAIL to ms_noti_db_update_complete\n");
			return 0;
		}
		else
		{
			printf("SUCCESS to ms_noti_db_update_complete\n");
		}

		return 0;
	}

* @endcode
*/

int ms_noti_update_complete(void);

/**
* @}
*/

#ifdef __cplusplus
}
#endif

#endif /*_MEDIA_UTIL_NOTI_H_*/
