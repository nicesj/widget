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

#include <widget_service_internal.h>

#ifdef WIDGET_FEATURE_GBAR_SUPPORTED
typedef struct widget_provider_event_callback { /**< Event callback table for widget Provider App */
    struct _widget { /**< Event callbacks for widget */
        int (*create)(const char *id, const char *content, int w, int h, void *data); /**< Creating a new widget */
        int (*resize)(const char *id, int w, int h, void *data); /**< Resizing the widget */
        int (*destroy)(const char *id, widget_destroy_type_e reason, void *data); /**< Destroying the widget */
    } widget;

    struct _glancebar { /**< Event callbacks for Glance Bar */
        int (*create)(const char *id, int w, int h, double x, double y, void *data); /**< Create a Glance Bar */
        int (*resize_move)(const char *id, int w, int h, double x, double y, void *data); /**< Resize & Move the Glance Bar */
        int (*destroy)(const char *id, int reason, void *data); /**< Destroy the Glance Bar */
    } gbar;    /**< Short from Glance Bar */

    int (*clicked)(const char *id, const char *event, double x, double y, double timestamp, void *data); /**< If viewer calls widget_click function, this event callback will be called */

    int (*update)(const char *id, const char *content, int force, void *data); /**< Updates a widget */
    int (*text_signal)(const char *id, const char *signal_name, const char *source, struct widget_event_info *info, void *data); /**< User events widget */

    int (*pause)(const char *id, void *data); /**< Pause */
    int (*resume)(const char *id, void *data); /**< Resume */

    void (*connected)(void *data); /**< When a provider is connected to master service, this callback will be called */
    void (*disconnected)(void *data); /**< When a provider is disconnected from the master service, this callback will be called */

    void *data; /**< Callback data */
} *widget_provider_event_callback_s;
#endif

/**
 * @internal
 * @brief Getting the package name of current running process.
 * @privlevel platform
 * @since_tizen 2.3.1
 * @return address of package name stored location
 * @retval @c NULL if it fails to get the package name
 */
extern const char *widget_provider_app_pkgname(void);

/**
 * @internal
 * @brief Set the control mode of master provider for provider apps.
 * @since_tizen 2.3.1
 * @param[in] flag 1 if the app's life-cycle will be managed by itself, or 0, the master will manage the life-cycle of a provider app
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_INVALID_PARAMETER_PARAMETER Invalid parameter
 * @retval #WIDGET_ERROR_FAULT Unrecoverable error
 * @retval #WIDGET_ERROR_NONE Successfully done
 */
extern int widget_provider_app_set_control_mode(int flag);

/**
 * @brief Send the "updated" event for widget manually
 * @since_tizen 2.3.1
 * @param[in] id Instance Id
 * @param[in] idx Index of updated buffer
 * @param[in] w Width
 * @param[in] h Height
 * @param[in] for_gbar 1 for sending updated event of GBAR or 0 for widget
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_INVALID_PARAMETER_PARAMETER Invalid argument
 * @retval #WIDGET_ERROR_OUT_OF_MEMORY Out of memory
 * @retval #WIDGET_ERROR_FAULT Unrecoverable error
 * @retval #WIDGET_ERROR_NONE Successfully sent
 * @see widget_provider_app_gbar_updated()
 */
extern int widget_provider_app_send_updated_event(const char *id, int idx, int x, int y, int w, int h, int for_gbar);

/**
 * @brief Send the "updated" event for widget manually
 * @since_tizen 2.3.1
 * @param[in] handle Buffer handler
 * @param[in] idx Index of updated buffer
 * @param[in] w Width
 * @param[in] h Height
 * @param[in] for_gbar 1 for sending updated event of GBAR or 0 for widget
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_INVALID_PARAMETER_PARAMETER Invalid argument
 * @retval #WIDGET_ERROR_OUT_OF_MEMORY Out of memory
 * @retval #WIDGET_ERROR_FAULT Unrecoverable error
 * @retval #WIDGET_ERROR_NONE Successfully sent
 * @see widget_provider_app_gbar_updated()
 */
extern int widget_provider_app_send_buffer_updated_event(widget_buffer_h handle, int idx, int x, int y, int w, int h, int for_gbar);

/**
 * @internal
 * @brief Creating a buffer handler for given widget instance.
 * @since_tizen 2.3.1
 *
 * @param[in] id widget instance Id
 * @param[in] for_pd 1 for Glance Bar or 0
 * @param[in] handle Event Handler. All events will be delivered via this event callback.
 * @param[in] data Callback Data
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return Buffer Object
 * @retval @c NULL if it fails to create a buffer
 * @retval addr Address of widget buffer object
 * @see widget_provider_app_acquire_buffer()
 * @see widget_provider_app_release_buffer()
 * @see widget_provider_app_destroy_buffer()
 */
extern widget_buffer_h widget_provider_app_create_buffer(const char *id, int for_pd, int (*handle)(widget_buffer_h, widget_buffer_event_data_t, void *), void *data);

/**
 * @internal
 * @brief Acquire buffer
 * @since_tizen 2.3.1
 * @param[in] handle widget Buffer Handle
 * @param[in] w Buffer Width
 * @param[in] h Buffer Height
 * @param[in] bpp Bytes Per Pixels
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_INVALID_PARAMETER_PARAMETER Invalid parameters are used
 * @retval #WIDGET_ERROR_FAULT Failed to acquire buffer. Unable to recover from error
 * @retval #WIDGET_ERROR_NONE If it creates buffer successfully
 * @pre widget_provider_app_create_buffer must be called first to create a handle
 */
extern int widget_provider_app_acquire_buffer(widget_buffer_h handle, int w, int h, int bpp);

/**
 * @internal
 * @brief Acquire extra buffer
 * @details if you need more buffer to rendering scenes, you can allocate new one using this.\n
 *          But the maximum count of allocatable buffer is limited. it depends on system setting.\n
 * @since_tizen 2.3.1
 * @param[in] handle widget Buffer handle
 * @param[in] idx Index of a buffer, 0 <= idx <= SYSTEM_DEFINE
 * @param[in] w Buffer width
 * @param[in] h Buffer height
 * @param[in] bpp Bytes Per Pixel
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_INVALID_PARAMETER_PARAMETER Invalid parameters are used
 * @retval #WIDGET_ERROR_FAULT Failed to acquire buffer. Unable to recover from error
 * @retval #WIDGET_ERROR_NONE If it creates buffer successfully
 * @pre widget_provider_app_create_buffer must be called first to create a handle
 */
extern int widget_provider_app_acquire_extra_buffer(widget_buffer_h handle, int idx, int w, int h, int bpp);

/**
 * @internal
 * @brief Release buffer
 * @since_tizen 2.3.1
 * @param[in] handle widget Buffer Handle
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_INVALID_PARAMETER_PARAMTER Invalid parameters are used
 * @retval #WIDGET_ERROR_FAULT Failed to release buffer, Unable to recover from error
 * @retval #WIDGET_ERROR_NOT_EXIST Given buffer handle is not exists
 * @retval #WIDGET_ERROR_NONE If it releases buffer successfully
 * @see widget_provider_app_create_buffer()
 * @see widget_provider_app_acquire_buffer()
 * @see widget_provider_app_destroy_buffer()
 */
extern int widget_provider_app_release_buffer(widget_buffer_h handle);

/**
 * @internal
 * @brief Release buffer
 * @since_tizen 2.3.1
 * @param[in] handle widget Buffer Handle
 * @param[in] idx Index of a buffer
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_INVALID_PARAMETER_PARAMTER Invalid parameters are used
 * @retval #WIDGET_ERROR_FAULT Failed to release buffer, Unable to recover from error
 * @retval #WIDGET_ERROR_NOT_EXIST Given buffer handle is not exists
 * @retval #WIDGET_ERROR_NONE If it releases buffer successfully
 * @see widget_provider_app_create_buffer()
 * @see widget_provider_app_acquire_buffer()
 * @see widget_provider_app_destroy_buffer()
 */
extern int widget_provider_app_release_extra_buffer(widget_buffer_h handle, int idx);

/**
 * @internal
 * @brief Destroy buffer
 * @since_tizen 2.3.1
 * @param[in] handle widget Buffer Handle
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_INVALID_PARAMETER_PARAMTER Invalid parameter
 * @retval #WIDGET_ERROR_NONE Successfully destroyed
 * @see widget_provider_app_create_buffer()
 * @see widget_provider_app_acquire_buffer()
 * @see widget_provider_app_destroy_buffer()
 * @pre widget_provider_app_create_buffer() must be successfully called and before call this, the buffer should be released first.
 */
extern int widget_provider_app_destroy_buffer(widget_buffer_h handle);

/**
 * @internal
 * @brief Get the resource Id
 * @since_tizen 2.3.1
 * * @param[in] handle widget Buffer Handle
 * @return unsigned int Resource Id
 * @retval 0 if it fails to get resource Id
 * @retval positive if it succeed to get the resource Id
 * @see widget_provider_app_create_buffer()
 * @see widget_provider_app_acquire_buffer()
 * @pre widget_provider_app_acquire_buffer() must be successfully called.
 */
extern unsigned int widget_provider_app_get_buffer_resource_id(widget_buffer_h handle);

/**
 * @internal
 * @brief Get the resource Id for extra buffer
 * @since_tizen 2.3.1
 * * @param[in] handle widget Buffer Handle
 * @return unsigned int Resource Id
 * @retval 0 if it fails to get resource Id
 * @retval positive if it succeed to get the resource Id
 * @see widget_provider_app_create_buffer()
 * @see widget_provider_app_acquire_buffer()
 * @pre widget_provider_app_acquire_buffer() must be successfully called.
 */
extern unsigned int widget_provider_app_get_extra_buffer_resource_id(widget_buffer_h handle, int idx);

/**
 * @internal
 * @brief Send a result of accessibility event processing
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @param[in] handle widget Buffer Handle
 * @param[in] status enum widget_access_status (widget_service.h)
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_INVALID_PARAMETER_PARAMETER Invalid parameter
 * @retval #WIDGET_ERROR_FAULT Failed to send access result status
 * @retval #WIDGET_ERROR_NONE Successfully sent
 * @see widget_provider_app_send_key_status()
 */
extern int widget_provider_app_send_access_status(widget_buffer_h handle, int status);

/**
 * @internal
 * @brief Send a result of key event processing
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @param[in] handle widget Buffer Handle
 * @param[in] status enum widget_key_status (widget_service.h)
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_INVALID_PARAMETER_PARAMETER Invalid parameter
 * @retval #WIDGET_ERROR_FAULT Failed to send key processing result status
 * @retval #WIDGET_ERROR_NONE Successfully sent
 * @see widget_provider_app_send_access_status()
 */
extern int widget_provider_app_send_key_status(widget_buffer_h handle, int status);

/**
 * @brief Send the request to freeze/thaw scroller to the home-screen
 * @details If a viewer doesn't implement service routine for this request, this doesn't make any changes.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/core/widget.provider
 * @param[in] id Instance Id
 * @param[in] seize 1 for freeze, 0 for thaw
 * @return 0 on success, otherwise a negative error value
 */
extern int widget_provider_app_hold_scroll(const char *id, int seize);

/**
 * @brief Initialize the provider app synchronously.
 * @since_tizen 2.3.1
 * @param[in] table Event callback table
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Succeed to initialize the provider app.
 * @retval #WIDGET_ERROR_FAULT Failed to initialize the provider app.
 * @retval #WIDGET_ERROR_PERMISSION Lack of access privileges
 */
extern int widget_provider_app_init_sync(widget_provider_event_callback_s table);

/**
 * @brief Send deleted event to the master to remove itself from the viewer app.
 * @since_tizen 2.3.1
 * @param[in] id Instance Id
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Succeed to send deleted event to master.
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid parameter.
 * @retval #WIDGET_ERROR_FAULT Failed to send deleted event.
 * @retval #WIDGET_ERROR_PERMISSION Lack of access privileges.
 */
extern int widget_provider_app_send_deleted(const char *id);

/**
 * @brief callback function subscribing 'clicked' event
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * */
typedef int (*widget_provider_clicked_cb)(const char *id, const char *event, double x, double y, double timestamp, void *data);

/**
 * @brief Destroying all widget instances locally, notify it to a developer who can handles it.
 * @since_size 2.3.1
 * @param[in] reason see #WIDGET_DESTROY_TYPE_XXX
 * @feature %http://tizen.org/feature/shell.appwidget
 * @return #WIDGET_ERROR_NONE on success,
 *         otherwise an error code (see #WIDGET_ERROR_XXX) on failure
 */
extern int widget_provider_app_terminate_app(widget_destroy_type_e reason, int destroy_instances);

/**
 * @brief Creating an application process.
 * @since_tizen 2.3.1
 * @feature %http://tizen.org/feature/shell.appwidget
 * @return #WIDGET_ERROR_NONE on success,
 *         otherwise an error code (see #WIDGET_ERROR_XXX) on failure
 */
extern int widget_provider_app_create_app(void);

extern int widget_provider_app_add_pre_callback(widget_pre_callback_e type, widget_pre_callback_t cb, void *data);

extern int widget_provider_app_del_pre_callback(widget_pre_callback_e type, widget_pre_callback_t cb, void *data);

extern int widget_provider_app_get_last_ctrl_mode(const char *id, int *cmd, int *value);

/* End of a file */
