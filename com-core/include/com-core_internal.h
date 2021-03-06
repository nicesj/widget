/*
 * Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
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

extern void invoke_con_cb_list(int server_fd, int handle, guint id, void *data, int watch);

/**
 * @note
 * This function will returns 0 if it is processed peacefully,
 * But 1 will be returned if it is already called. (in case of the nested call)
 */
extern int invoke_disconn_cb_list(int handle, int remove_id, int remove_data, int watch);

/* End of a file */
