/*
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
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

#ifndef _MEDIA_UTIL_DCM_H_
#define _MEDIA_UTIL_DCM_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*FaceFunc) (int error_code, int count, void* data);

int dcm_request_extract_all(uid_t uid);
int dcm_request_extract_media(const char *path, uid_t uid);
int dcm_request_extract_face_async(const unsigned int request_id, const char *path, FaceFunc func, void *user_data, uid_t uid);
int dcm_request_cancel_face(const unsigned int request_id, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* _MEDIA_UTIL_DCM_H_ */
