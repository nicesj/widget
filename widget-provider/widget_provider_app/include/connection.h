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

enum connection_event_type {
    CONNECTION_EVENT_TYPE_CONNECTED = 0x00,
    CONNECTION_EVENT_TYPE_DISCONNECTED = 0x01,
    CONNECTION_EVENT_TYPE_UNKNOWN = 0xFF,
};

struct connection;

extern int connection_init(void);
extern int connection_fini(void);

extern struct connection *connection_create(const char *addr, void *table);

extern struct connection *connection_find_by_addr(const char *addr);
extern struct connection *connection_find_by_fd(int fd);

extern int connection_add_event_handler(enum connection_event_type type, int (*event_cb)(int handle, void *data), void *data);
extern void *connection_del_event_handler(enum connection_event_type type, int (*event_cb)(int handle, void *data));

extern struct connection *connection_unref(struct connection *handle);
extern struct connection *connection_ref(struct connection *handle);

extern int connection_handle(struct connection *connection);
extern const char *connection_addr(struct connection *connection);

extern int connection_set_data(struct connection *connection, const char *key, void *data);
extern void *connection_del_data(struct connection *connection, const char *key);
extern void *connection_get_data(struct connection *connection, const char *key);

extern int connection_is_valid(struct connection *connection);

/* End of a file */

#define connection_destroy(handle)    connection_unref(handle);

/* End of a file */
