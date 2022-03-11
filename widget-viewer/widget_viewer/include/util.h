/*
 * Copyright 2013  Samsung Electronics Co., Ltd
 *
 * Licensed under the Flora License, Version 1.1 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://floralicense.org/license/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

extern int util_check_extension(const char *filename, const char *check_ptr);
extern double util_timestamp(void);
extern const char *util_uri_to_path(const char *uri);
extern int util_unlink(const char *filename);

#define SCHEMA_FILE   "file://"
#define SCHEMA_PIXMAP "pixmap://"
#define SCHEMA_SHM    "shm://"

#define container_of(ptr, type, member) \
        ({ const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

/* End of a file */
