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

extern int master_rpc_async_request(struct widget *handler, struct packet *packet, int urgent, void (*ret_cb)(struct widget *handler, const struct packet *result, void *data), void *data);
extern int master_rpc_sync_request(struct packet *packet);
extern void master_rpc_check_and_fire_consumer(void);
extern int master_rpc_request_only(struct widget *handler, struct packet *packet);
extern int master_rpc_clear_fault_package(const char *pkgname);
extern int master_rpc_clear_all_request(void);

/* End of a file */
