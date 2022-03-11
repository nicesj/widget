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

#include <widget_service.h>
#include <widget_service_internal.h>

#ifndef __WIDGET_PROVIDER_APP_H
#define __WIDGET_PROVIDER_APP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file widget_provider_app.h
 * @brief This file declares API of libwidget_provider_app library
 * @since_tizen 2.3.1
 */

/**
 * @addtogroup WIDGET_PROVIDER_APP_MODULE
 * @{
 */


/**
 * @brief Called to create a new widget
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @param[in] id widget instance id
 * @param[in] content_info Content info, this content info will be managed by viewer
 * @param[in] w width of the widget
 * @param[in] h height of the widget
 * @param[in] data user data
 * @return 0 on success, otherwise a negative error value
 * */
typedef int (*widget_provider_create_cb)(const char *id, const char *content, int w, int h, void *data);

/**
 * @brief Called to destroy a widget
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @param[in] id widget instance id
 * @param[in] reason reason of destroy
 * @return 0 on success, otherwise a negative error value
 * @see widget_destroy_type_e
 * */
typedef int (*widget_provider_destroy_cb)(const char *id, widget_destroy_type_e reason, void *data);

/**
 * @brief Called to pause a widget
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @param[in] id widget instance id
 * @param[in] data user data
 * @return 0 on success, otherwise a negative error value
  * */
typedef int (*widget_provider_pause_cb)(const char *id, void *data);

/**
 * @brief Called to resume a widget
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @param[in] id widget instance id
 * @param[in] data user data
 * @return 0 on success, otherwise a negative error value
 * */
typedef int (*widget_provider_resume_cb)(const char *id, void *data);

/**
 * @brief Called to resize a widget
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @param[in] id widget instance id
 * @param[in] w width of the widget
 * @param[in] h height of the widget
 * @param[in] data user data * @return 0 on success, otherwise a negative error value
 * */
typedef int (*widget_provider_resize_cb)(const char *id, int w, int h, void *data);

/**
 * @brief Called to update a widget
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @param[in] id widget instance id
 * @param[in] content_info Content info, this content info will be managed by viewer
 * @param[in] force force to update option
 * @param[in] data user data
 * @return 0 on success, otherwise a negative error value
 * */
typedef int (*widget_provider_update_cb)(const char *id, const char *content, int force, void *data);

/**
 * @brief Called when a text signal even emitted
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @param[in] id widget instance id
 * @param[in] signal_name name of the text signal
 * @param[in] source source string
 * @param[in] info position and region information of the text signal
 * @param[in] data user data
 * @return 0 on success, otherwise a negative error value
 * @see #widget_event_info_s
 * @see #WIDGET_TEXT_SIGNAL_NAME_EDIT_MODE_ON
 * @see #WIDGET_TEXT_SIGNAL_NAME_EDIT_MODE_OFF
  * */
typedef int (*widget_provider_text_signal_cb)(const char *id, const char *signal_name, const char *source, struct widget_event_info *info, void *data);

/**
 * @brief Called when a provider is connected to master service
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @param[in] data user data
 * @return 0 on success, otherwise a negative error value
 * */
typedef void (*widget_provider_connected_cb)(void *data);

/**
 * @brief Called when a provider is disconnected from the master service
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @param[in] data user data
 * @return 0 on success, otherwise a negative error value
 * */
typedef void (*widget_provider_disconnected_cb)(void *data);

/**
 * @brief Called when the orientation of the viewer is changed.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @param[in] id widget instance id
 * @param[in] degree orientation value (0 ~ 360)
 * @param[in] data user data
 * @return 0 on success, otherwise a negative error value
 */
typedef void (*widget_provider_orientation_changed_cb)(const char *id, int degree, void *data);

/**
 */
typedef void (*widget_provider_ctrl_mode_cb)(const char *id, int cmd, int value, void *data);

/**
 * @brief Widget event callback table
 * @since_tizen 2.3.1
 */
typedef struct widget_provider_event_callback {
	widget_provider_create_cb create;             /**< Called to create a new widget */
	widget_provider_destroy_cb destroy;           /**< Called to destroy a widget */
	widget_provider_pause_cb pause;               /**< Called to pause a widget */
	widget_provider_resume_cb resume;             /**< Called to resume a widget */
	widget_provider_resize_cb resize;             /**< Called to resize a widget */
	widget_provider_update_cb update;             /**< Called to update a widget */
	widget_provider_text_signal_cb text_signal;   /**< Called when a text signal even emitted */
	widget_provider_connected_cb connected;       /**< Called when a provider is connected to master service */
	widget_provider_disconnected_cb disconnected; /**< Called when a provider is disconnected from the master service */
	widget_provider_orientation_changed_cb orientation;
	widget_provider_ctrl_mode_cb ctrl_mode;
    void *data; /**< Callback data */
} *widget_provider_event_callback_s;

/**
 * @brief Initializes the provider app.
 * @since_tizen 2.3.1
 * @param[in] app_control app_control handle which is comes from app_control event callback
 * @param[in[ table Event callback table
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Succeed to initialize the provider app.
 * @retval #WIDGET_ERROR_FAULT Failed to initialize the provider app.
 * @retval #WIDGET_ERROR_PERMISSION Lack of access privileges
 */
extern int widget_provider_app_init(app_control_h app_control, widget_provider_event_callback_s table);

/**
 * @brief Checks the initialization status of widget application app.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return initialization status
 * @retval true if it is initialized
 * @retval false if it is not initialized
 */
extern bool widget_provider_app_is_initialized(void);

/**
 * @brief Finalizes the provider app
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privielge/widget.provider
 */
extern void widget_provider_app_fini(void);

/**
 * @brief Sends the extra info to the widget manually\n
 * @since_tizen 2.3.1
 * @param[in] id Widget instance id
 * @param[in] content_info Content info, this content info will be managed by viewer.
 * @param[in] title When an accessibility mode is turned on, this string will be read.
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_INVALID_PARAMETER_PARAMETER Invalid argument
 * @retval #WIDGET_ERROR_OUT_OF_MEMORY Out of memory
 * @retval #WIDGET_ERROR_FAULT Unrecoverable error
 * @retval #WIDGET_ERROR_NONE Successfully sent
 * @see #widget_provider_app_widget_updated
 * @see #WIDGET_SMART_SIGNAL_EXTRA_INFO_UPDATED
 */
extern int widget_provider_app_send_extra_info(const char *id, const char *content_info, const char *title);

/**
 * @brief Sets private data to given instance.
 * @since_tizen 2.3.1
 * * @param[in] id Widget instance id
 * @param[in] data Private Data
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_INVALID_PARAMETER_PARAMETER Invalid argument
 * @retval #WIDGET_ERROR_OUT_OF_MEMORY Out of memory
 * @retval #WIDGET_ERROR_NOT_EXIST Instance is not exists
 * @retval #WIDGET_ERROR_NONE Successfully registered
 * @see #widget_provider_app_data
 */
extern int widget_provider_app_set_data(const char *id, void *data);

/**
 * @brief Gets the private data from given instance.
 * @since_tizen 2.3.1
 * @param[in] id Instance Id
 * @retval Address Private data
 * @retval @c NULL fails to get private data or not registered, get_last_result() will returns reason of failure.
 * @see #widget_widget_provider_app_set_data
 */
extern void *widget_provider_app_get_data(const char *id);

/**
 * @brief Gets the list of private data
 * @details This function will returns list of data, list type is Eina_List.
 * @since_tizen 2.3.1
 * @return void* Eina_List *
 * @retval Address Eina_List *
 * @retval @c NULL if there is no list of data, get_last_result() will returns reason of failure.
 * @post Returned list must has to be freed by function eina_list_free or macro EINA_LIST_FREE
 * @see #widget_provider_app_set_data
 */
extern void *widget_provider_app_get_data_list(void);

/**
 * @brief Gets the current orientation value
 * @details This function will returns current orientation value.
 * @since_tizen 2.3.1
 * @param[in] id widget instance id
 * @return int degree (0 ~ 360)
 * @retval @c 0 is the default orientation
 */
extern int widget_provider_app_get_orientation(const char *id);

/**
 * @brief Gets the widget id using given instance id
 * @details This function will returns widget id which is allocated in the heap.
 * @since_tizen 2.3.1
 * @param[in] id widget instance id
 * @return char * widget id allocated in heap
 * @retval @c NULL if it fails to allocate heap or cannot find a widget id of given instance id
 * @post Returned widget id should be freed using "free" function.
 */
extern char *widget_provider_app_get_widget_id(const char *id);

extern int widget_provider_app_get_last_ctrl(const char *id, int *cmd, int *value);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif

/* End of a file */
