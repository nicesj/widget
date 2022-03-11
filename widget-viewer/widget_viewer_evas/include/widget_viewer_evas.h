/*
 * Samsung API
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Flora License, Version 1.1 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://floralicense.org/license/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __WIDGET_VIEWER_EVAS_H
#define __WIDGET_VIEWER_EVAS_H

#include <tizen_type.h>

#include <widget_service.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file widget_viewer_evas.h
 * @brief  This file declares API of libwidget-viewer-evas library
 * @since_tizen 2.3.1
 */

/**
 * @addtogroup CAPI_WIDGET_VIEWER_EVAS_MODULE
 * @{
 */

/**
 * @since_tizen 2.3.1
 * @brief Default refresh interval of widgets.
 * @see #widget_viewer_evas_add_widget
 * @see #widget_viewer_evas_get_period
 */
#define WIDGET_VIEWER_EVAS_DEFAULT_PERIOD                -1.0f

/**
 * @since_tizen 2.3.1
 * @brief Event name for smart callback of widget events: Widget creation is aborted
 * @see #widget_evas_event_info_s
 * @see evas_object_smart_callback_add
 */
#define WIDGET_SMART_SIGNAL_WIDGET_CREATE_ABORTED   "widget,create,aborted"

/**
 * @since_tizen 2.3.1
 * @brief Event name for smart callback of widget events: Widget is created
 * @see #widget_evas_event_info_s
 * @see evas_object_smart_callback_add
 */
#define WIDGET_SMART_SIGNAL_WIDGET_CREATED          "widget,created"

/**
 * @since_tizen 2.3.1
 * @brief Event name for smart callback of widget events: Resizing widget is aborted
 * @see #widget_evas_event_info_s
 * @see evas_object_smart_callback_add
 */
#define WIDGET_SMART_SIGNAL_WIDGET_RESIZE_ABORTED   "widget,resize,aborted"

/**
 * @since_tizen 2.3.1
 * @brief Event name for smart callback of widget events: Widget is resized
 * @see #widget_evas_event_info_s
 * @see evas_object_smart_callback_add
 */
#define WIDGET_SMART_SIGNAL_WIDGET_RESIZED          "widget,resized"

/**
 * @since_tizen 2.3.1
 * @brief Event name for smart callback of widget events: Widget has faulted
 * @see #widget_evas_event_info_s
 * @see evas_object_smart_callback_add
 */
#define WIDGET_SMART_SIGNAL_WIDGET_FAULTED          "widget,faulted"

/**
 * @since_tizen 2.3.1
 * @brief Event name for smart callback of widget events: Widget content is updated
 * @see #widget_evas_event_info_s
 * @see evas_object_smart_callback_add
 */
#define WIDGET_SMART_SIGNAL_UPDATED                 "updated"

/**
 * @since_tizen 2.3.1
 * @brief Event name for smart callback of widget events: Widget extra info is updated
 * @see #widget_evas_event_info_s
 * @see evas_object_smart_callback_add
 */
#define WIDGET_SMART_SIGNAL_EXTRA_INFO_UPDATED      "info,updated"

/**
 * @since_tizen 2.3.1
 * @brief Event name for smart callback of widget events: Provider is disconnected
 * @see #widget_evas_event_info_s
 * @see evas_object_smart_callback_add
 */
#define WIDGET_SMART_SIGNAL_PROVIDER_DISCONNECTED   "provider,disconnected"

/**
 * @since_tizen 2.3.1
 * @brief Event name for smart callback of widget events: Control Scroller
 * @see #widget_evas_event_info_s
 * @see evas_object_smart_callback_add
 */
#define WIDGET_SMART_SIGNAL_CONTROL_SCROLLER        "control,scroller"

/**
 * @since_tizen 2.3.1
 * @brief Event name for smart callback of widget events: Widget is deleted
 * @see #widget_evas_event_info_s
 * @see evas_object_smart_callback_add
 */
#define WIDGET_SMART_SIGNAL_WIDGET_DELETED          "widget,deleted"

/**
 * @since_tizen 2.3.1
 * @brief Event name for smart callback of widget events: Period is changed
 * @see #widget_evas_event_info_s
 * @see evas_object_smart_callback_add
 */
#define WIDGET_SMART_SIGNAL_PERIOD_CHANGED          "widget,period,changed"

/**
 * @since_tizen 2.3.1
 * @brief Data structure which will be sent as a parameter of smart callback for signals WIDGET_SMART_SIGNAL_XXX
 * @see #WIDGET_SMART_SIGNAL_WIDGET_CREATE_ABORTED
 * @see #WIDGET_SMART_SIGNAL_WIDGET_CREATED
 * @see #WIDGET_SMART_SIGNAL_WIDGET_RESIZE_ABORTED
 * @see #WIDGET_SMART_SIGNAL_WIDGET_RESIZED
 * @see #WIDGET_SMART_SIGNAL_WIDGET_FAULTED
 * @see #WIDGET_SMART_SIGNAL_UPDATED
 * @see #WIDGET_SMART_SIGNAL_EXTRA_INFO_UPDATED
 * @see #WIDGET_SMART_SIGNAL_PROVIDER_DISCONNECTED
 * @see #WIDGET_SMART_SIGNAL_CONTROL_SCROLLER
 * @see #WIDGET_SMART_SIGNAL_WIDGET_DELETED
 * @see #WIDGET_SMART_SIGNAL_PERIOD_CHANGED
 */
typedef struct widget_evas_event_info {
    const char *widget_app_id;       /**< Widget application id */
    widget_event_type_e event; /**< Event type for detail event information - WIDGET_EVENT_XXX, refer the widget_serivce.h */
    int error;		           /**< Error type - WIDGET_ERROR_XXX, refer the widget_errno.h */
} widget_evas_event_info_s;

/**
 * @brief Enumerations for setting visibility status of a widget.
 * @since_tizen 2.3.1
 * @see #widget_viewer_evas_freeze_visibility
 */
typedef enum widget_visibility_status {
	WIDGET_VISIBILITY_STATUS_SHOW_FIXED = 1,  /**< Visibility of the widget will be fixed as 'SHOW'*/
	WIDGET_VISIBILITY_STATUS_HIDE_FIXED = 2   /**< Visibility of the widget will be fixed as 'HIDE'*/
} widget_visibility_status_e;

/**
 * @brief Configuration keys
 * @since_tizen 2.3.1
 * @see #widget_viewer_evas_set_option
 */
typedef enum widget_evas_conf {
    WIDGET_VIEWER_EVAS_MANUAL_PAUSE_RESUME = 0x0001,   /**< Visibility will be changed manually. 1 : on, 0 : off */
    WIDGET_VIEWER_EVAS_USE_FIXED_SIZE = 0x0008,   /**< Widget will be resized to specific size only. 1 : on, 0 : off */
    WIDGET_VIEWER_EVAS_EASY_MODE = 0x0010,   /**< Easy mode on/off. 1 : on, 0 : off */
    WIDGET_VIEWER_EVAS_SCROLL_X = 0x0020,   /**< Box will be scrolled from left to right vice versa. 1 : on, 0 : off */
    WIDGET_VIEWER_EVAS_SCROLL_Y = 0x0040,   /**< Box will be scrolled from top to bottom vice versa. 1 : on, 0 : off */
    WIDGET_VIEWER_EVAS_EVENT_AUTO_FEED = 0x0080,   /**< Feeds event automatically from the master provider. 1 : on, 0 : off */
    WIDGET_VIEWER_EVAS_DELAYED_RESUME = 0x0100,   /**< Delaying the pause/resume when it is automatically changed. 1 : on, 0 : off */
    WIDGET_VIEWER_EVAS_SENSITIVE_MOVE = 0x0200,   /**< Force feeds mouse up event if the box is moved. 1 : on, 0 : off */
    WIDGET_VIEWER_EVAS_AUTO_RENDER_SELECTION = 0x0400, /**< Select render automatically, if a box moved, do not sync using animator, or use the animator. 1 : on, 0 : off */
    WIDGET_VIEWER_EVAS_DIRECT_UPDATE = 0x0800,   /**< Enable direct update path. 1 : on, 0 : off */
    WIDGET_VIEWER_EVAS_USE_RENDER_ANIMATOR = 0x1000,   /**< Use the render animator or not. 1 : on, 0 : off */
	WIDGET_VIEWER_EVAS_SKIP_ACQUIRE = 0x2000,   /**< Even if the viewer cannot get acquired resource id, try to update using default one. 1 : on, 0 : off */
    WIDGET_VIEWER_EVAS_UNKNOWN = 0xFFFF
} widget_evas_conf_e;

/**
 * @brief Initializes the widget system
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] win Window object
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE If success
 * @retval #WIDGET_ERROR_INVALID_PARAMETER
 * @retval #WIDGET_ERROR_ALREADY_EXIST Already initialized
 * @retval #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @see #widget_viewer_evas_fini
 */
extern int widget_viewer_evas_init(Evas_Object *win);

/**
 * @brief Finalizes the widget system
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE If success
 * @retval #WIDGET_ERROR_FAULT Unrecoverable error occurred
 * @retval #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @see #widget_viewer_evas_init
 */
extern int widget_viewer_evas_fini(void);

/**
 * @brief Creates a new widget object
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @remarks The specific error code can be obtained using the get_last_result() method. Error codes are described in Exception section.
 * @param[in] parent Evas Object of parent
 * @param[in] widget_id widget id
 * @param[in] content_info Contents that will be given to the widget instance
 * @param[in] period Update period (@c WIDGET_DEFAULT_PERIOD can be used for this; this argument will be used to specify the period of updating contents of a widget)
 * @return Widget Object. NULL on error
 * @exception #WIDGET_ERROR_NONE Successfully added
 * @exception #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @exception #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @see get_last_result()
 * @see #widget_service_get_widget_id
 */
extern Evas_Object *widget_viewer_evas_add_widget(Evas_Object *parent, const char *widget_id, const char *content_info, double period);

/**
 * @brief Notifies the status of the viewer to all providers
 * @details If you call this, all providers will gets "resumed" event.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE if success
 * @retval #WIDGET_ERROR_FAULT if it failed to send state (paused) info
 * @retval #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @see widget_viewer_evas_notify_paused_status_of_viewer()
 */
extern int widget_viewer_evas_notify_resumed_status_of_viewer(void);

/**
 * @brief Notifies the status of the viewer to all providers
 * @details If you call this, all providers will gets "paused" event.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE if success
 * @retval #WIDGET_ERROR_FAULT if it failed to send state (resumed) info
 * @retval #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @see widget_viewer_evas_notify_resumed_status_of_viewer()
 */
extern int widget_viewer_evas_notify_paused_status_of_viewer(void);

/**
 * @brief Notifies the orientation of the viewer to all providers
 * @details If you call this, all providers will gets "rotated" event.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] orientation orientation of viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE if success
 * @retval #WIDGET_ERROR_FAULT if it failed to send state (resumed) info
 * @retval #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 */
extern int widget_viewer_evas_notify_orientation_of_viewer(int orientation);

/**
 * @brief Pauses given widget.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] widget a widget object
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE if success
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @retval #WIDGET_ERROR_FAULT if it failed to send state (resumed) info
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 */
extern int widget_viewer_evas_pause_widget(Evas_Object *widget);

/**
 * @brief Resume given widget.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] widget a widget object
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE if success
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_ERROR_FAULT if it failed to send state (resumed) info
 * @retval #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 */
extern int widget_viewer_evas_resume_widget(Evas_Object *widget);

/**
 * @brief Changes the configurable values of widget system
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] type Configuration item
 * @param[in] value Its value
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE if success
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @see #widget_evas_conf
 */
extern int widget_viewer_evas_set_option(widget_evas_conf_e type, int value);

/**
 * @brief Gets content string of widget
 * @details This string can be used for creating contents of widget again after reboot a device or recovered from crash(abnormal status)
 * @remarks Returned string should not be freed.
 * @remarks The specific error code can be obtained using the get_last_result() method. Error codes are described in Exception section.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] widget a widget object
 * @return content string to be recognize content of the widget
 * @retval NULL if there is no specific content string.
 * @exception #WIDGET_ERROR_NONE Successfully get content string
 * @exception #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @exception #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @see get_last_result()
 * @post Returned string should not be freed
 */
extern const char *widget_viewer_evas_get_content_info(Evas_Object *widget);

/**
 * @brief Gets summarized string of the widget content for accessibility.
 * @details If the accessibility feature is turned on, a viewer can use this text to describe the widget.
 * @remarks Returned string should not be freed.
 * @remarks The specific error code can be obtained using the get_last_result() method. Error codes are described in Exception section.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] widget a widget object
 * @return title string to be used for summarizing the widget
 * @retval NULL if there is no summarized text for content of given widget.
 * @exception #WIDGET_ERROR_NONE Successfully get title string
 * @exception #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @exception #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @see get_last_result()
 */
extern const char *widget_viewer_evas_get_title_string(Evas_Object *widget);

/**
 * @brief Gets the id of the widget
 * @remarks Returned string should not be freed.
 * @remarks The specific error code can be obtained using the get_last_result() method. Error codes are described in Exception section.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] widget a widget object
 * @return widget id
 * @retval NULL if an error occurred and you can get the reason of failure using get_last_result()
 * @exception #WIDGET_ERROR_NONE Successfully get widget id
 * @exception #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @exception #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @see get_last_result()
 */
extern const char *widget_viewer_evas_get_widget_id(Evas_Object *widget);

/**
 * @brief Gets the update period of the widget.
 * @remarks The specific error code can be obtained using the get_last_result() method. Error codes are described in Exception section.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] widget a widget object
 * @return period the update period of the widget.
 * @retval the update interval of the widget
 * @exception #WIDGET_ERROR_NONE Successfully get period
 * @exception #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @exception #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @see get_last_result()
 */
extern double widget_viewer_evas_get_period(Evas_Object *widget);

/**
 * @brief Cancels click event procedure.
 * @remarks The specific error code can be obtained using the get_last_result() method. Error codes are described in Exception section.
 * @details If you call this function after feed the mouse_down(or mouse_set) event, the widget will get ON_HOLD events.\n
 *          If a widget gets ON_HOLD event, it will not do anything even if you feed mouse_up(or mouse_unset) event.\n
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] widget a widget object
 * @exception #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @exception #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @see get_last_result()
 *
 */
extern void widget_viewer_evas_cancel_click_event(Evas_Object *widget);

/**
 * @brief Hides the preview of the widget
 * @remarks This function should be called right after create the widget object before resizing it
 * @remarks The specific error code can be obtained using the get_last_result() method. Error codes are described in Exception section.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] widget a widget object
 * @exception #WIDGET_ERROR_NONE Successfully disabled preview
 * @exception #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @exception #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @see get_last_result()
 */
extern void widget_viewer_evas_disable_preview(Evas_Object *widget);

/**
 * @brief Hides the help text of the widget
 * @details While loading a box, hide the help text
 * @remarks This function should be called right after create the widget object before resizing it
 * @remarks The specific error code can be obtained using the get_last_result() method. Error codes are described in Exception section.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] widget a widget object
 * @exception #WIDGET_ERROR_NONE Successfully disabled overlay text
 * @exception #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @exception #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @see get_last_result()
 */
extern void widget_viewer_evas_disable_overlay_text(Evas_Object *widget);

/**
 * @brief Hides the loading message of the widget
 * @details if you disable it, there is no preview & help text while creating a widget object
 * @remarks This function should be called right after create the widget object before resizing it
 * @remarks The specific error code can be obtained using the get_last_result() method. Error codes are described in Exception section.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] widget a widget object
 * @exception #WIDGET_ERROR_NONE Successfully disabled loading text
 * @exception #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @exception #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @see get_last_result()
 */
extern void widget_viewer_evas_disable_loading(Evas_Object *widget);

/**
 * @brief Feeds the mouse_up event to the provider of the widget
 * @details This is very similar with widget_viewer_evas_cancel_click(), but this will sends mouse_up event explicitly.\n
 *          Also feed the ON_HOLD event before feeds mouse_up event.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] widget a widget object
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE if success
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @retval #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 */
extern int widget_viewer_evas_feed_mouse_up_event(Evas_Object *widget);

/**
 * @brief Activate a widget in faulted state.
 * @details A widget in faulted state MUST be activated before adding the widget.
 * @remarks The specific error code can be obtained using the get_last_result() method. Error codes are described in Exception section.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] widget a widget object faulted
 * @exception #WIDGET_ERROR_NONE Successfully activate faulted widget
 * @exception #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @exception #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @see get_last_result()
 */
extern void widget_viewer_evas_activate_faulted_widget(Evas_Object *widget);

/**
 * @brief Check whether the widget is faulted.
 * @remarks The specific error code can be obtained using the get_last_result() method. Error codes are described in Exception section.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] widget a widget object
 * @return faulted state of the widget and you can get the result state of this function by using get_last_result()
 * @retval true for faulted state
 * @retval false for not faulted state
 * @exception #WIDGET_ERROR_NONE Successfully get the faulted state of the widget
 * @exception #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @exception #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @see get_last_result()
 */
extern bool widget_viewer_evas_is_faulted(Evas_Object *widget);

/**
 * @brief Freezes visibility of the widget
 * @details If you don't want to change the visibility automatically, freeze it.\n
 *        The visibility will not be changed even though a box disappeared(hidden)/displayed(shown) from/on the screen.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] widget a widget object
 * @param[in] status a visibility status of the widget
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE if success
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @retval #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @see #widget_visibility_status_e
 */
extern int widget_viewer_evas_freeze_visibility(Evas_Object *widget, widget_visibility_status_e status);

/**
 * @brief Thaws visibility of the widget
 * @details If you want to let the visibility change automatically again, call this function.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] widget a widget object
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE if success
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @retval #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 */
extern int widget_viewer_evas_thaw_visibility(Evas_Object *widget);

/**
 * @brief Get the frozen state of visibility option.
 * @remarks The specific error code can be obtained using the get_last_result() method. Error codes are described in Exception section.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] widget a widget object
 * @return fixed state of visibility and you can get the result state of this function by using get_last_result()
 * @retval true for frozen state
 * @retval false for not frozen state
 * @exception #WIDGET_ERROR_NONE Successfully get the state of visibility
 * @exception #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @exception #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @see get_last_result()
 */
extern bool widget_viewer_evas_is_visibility_frozen(Evas_Object *widget);

/**
 * @brief Validate the object, whether it is a widget object or not
 * @remarks The specific error code can be obtained using the get_last_result() method. Error codes are described in Exception section.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] widget a widget object
 * @return result of validation and you can get the result state of this function by using get_last_result()
 * @retval true this is a widget
 * @retval false this is not a widget
 * @exception #WIDGET_ERROR_NONE Successfully get result of validation
 * @exception #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @exception #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @see get_last_result()
 */
extern bool widget_viewer_evas_is_widget(Evas_Object *widget);

/**
 * @brief Before delete a widget, set the deletion mode
 * @remarks The specific error code can be obtained using the get_last_result() method. Error codes are described in Exception section.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] widget a widget object which will be deleted soon
 * @param[in] flag Pass 1 if you delete this widget instance permanently, or pass 0 if you want to keep it and it will be re-created soon.
 * @exception #WIDGET_ERROR_NONE Successfully set the flag
 * @exception #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @exception #WIDGET_ERROR_PERMISSION_DENIED Permission denied
 * @see get_last_result()
 */
extern void widget_viewer_evas_set_permanent_delete(Evas_Object *widget, int flag);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif

/* End of a file */
