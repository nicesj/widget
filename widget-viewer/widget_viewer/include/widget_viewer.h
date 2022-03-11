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

#ifndef __WIDGET_VIEWER_H
#define __WIDGET_VIEWER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file widget_viewer.h
 * @brief This file declares API of libwidget-viewer library
 * @since_tizen 2.3.1
 */

/**
 * @addtogroup CAPI_WIDGET_VIEWER_MODULE
 * @{
 */

/**
 * @brief Structure definition for a widget instance.
 * @since_tizen 2.3.1
 */
typedef struct widget *widget_h;

/**
 * @internal
 * @brief Definition for a default update period for widget (defined in the package manifest file).
 * @since_tizen 2.3.1
 */
#define WIDGET_DEFAULT_PERIOD -1.0f

#define WIDGET_VIEWER_CLICK_BUTTON_LEFT    "clicked"
#define WIDGET_VIEWER_CLICK_BUTTON_RIGHT   "clicked,1"
#define WIDGET_VIEWER_CLICK_BUTTON_CENTER  "clicked,2"

/**
 * @internal
 * @brief Enumeration for Mouse & Key event for buffer type widget or Glance Bar.
 * @details Viewer should send these events to widget.
 * @since_tizen 2.3.1
 */
typedef enum widget_mouse_event_type {
    WIDGET_MOUSE_EVENT_MASK       = 0x20000000, /**< Mask value for mouse event */
    WIDGET_MOUSE_EVENT_GBAR_MASK  = 0x10000000, /**< Mask value for Glance Bar event */
    WIDGET_MOUSE_EVENT_WIDGET_MASK  = 0x40000000, /**< Mask value for widget event */

    WIDGET_MOUSE_EVENT_DOWN       = 0x00000001, /**< widget mouse down event for widget */
    WIDGET_MOUSE_EVENT_UP         = 0x00000002, /**< widget mouse up event for widget */
    WIDGET_MOUSE_EVENT_MOVE       = 0x00000004, /**< widget mouse move event for widget */
    WIDGET_MOUSE_EVENT_ENTER      = 0x00000008, /**< widget mouse enter event for widget */
    WIDGET_MOUSE_EVENT_LEAVE      = 0x00000010, /**< widget mouse leave event for widget */
    WIDGET_MOUSE_EVENT_SET        = 0x00000020, /**< widget mouse set auto event for widget */
    WIDGET_MOUSE_EVENT_UNSET      = 0x00000040, /**< widget mouse unset auto event for widget */

    WIDGET_MOUSE_EVENT_ON_SCROLL  = 0x00000080, /**< widget On scrolling */
    WIDGET_MOUSE_EVENT_ON_HOLD    = 0x00000100, /**< widget On holding */
    WIDGET_MOUSE_EVENT_OFF_SCROLL = 0x00000200, /**< widget Stop scrolling */
    WIDGET_MOUSE_EVENT_OFF_HOLD   = 0x00000400, /**< widget Stop holding */

    WIDGET_MOUSE_ON_SCROLL        = WIDGET_MOUSE_EVENT_WIDGET_MASK | WIDGET_MOUSE_EVENT_MASK | WIDGET_MOUSE_EVENT_ON_SCROLL, /**< Mouse event occurs while scrolling */
    WIDGET_MOUSE_ON_HOLD          = WIDGET_MOUSE_EVENT_WIDGET_MASK | WIDGET_MOUSE_EVENT_MASK | WIDGET_MOUSE_EVENT_ON_HOLD, /**< Mouse event occurs on holding */
    WIDGET_MOUSE_OFF_SCROLL       = WIDGET_MOUSE_EVENT_WIDGET_MASK | WIDGET_MOUSE_EVENT_MASK | WIDGET_MOUSE_EVENT_OFF_SCROLL, /**< Scrolling stopped */
    WIDGET_MOUSE_OFF_HOLD         = WIDGET_MOUSE_EVENT_WIDGET_MASK | WIDGET_MOUSE_EVENT_MASK | WIDGET_MOUSE_EVENT_OFF_HOLD, /**< Holding stopped */

    WIDGET_MOUSE_DOWN             = WIDGET_MOUSE_EVENT_WIDGET_MASK | WIDGET_MOUSE_EVENT_MASK | WIDGET_MOUSE_EVENT_DOWN, /**< Mouse down on the widget */
    WIDGET_MOUSE_UP               = WIDGET_MOUSE_EVENT_WIDGET_MASK | WIDGET_MOUSE_EVENT_MASK | WIDGET_MOUSE_EVENT_UP, /**< Mouse up on the widget */
    WIDGET_MOUSE_MOVE             = WIDGET_MOUSE_EVENT_WIDGET_MASK | WIDGET_MOUSE_EVENT_MASK | WIDGET_MOUSE_EVENT_MOVE, /**< Move move on the widget */
    WIDGET_MOUSE_ENTER            = WIDGET_MOUSE_EVENT_WIDGET_MASK | WIDGET_MOUSE_EVENT_MASK | WIDGET_MOUSE_EVENT_ENTER, /**< Mouse enter to the widget */
    WIDGET_MOUSE_LEAVE            = WIDGET_MOUSE_EVENT_WIDGET_MASK | WIDGET_MOUSE_EVENT_MASK | WIDGET_MOUSE_EVENT_LEAVE, /**< Mouse leave from the widget */
    WIDGET_MOUSE_SET              = WIDGET_MOUSE_EVENT_WIDGET_MASK | WIDGET_MOUSE_EVENT_MASK | WIDGET_MOUSE_EVENT_SET, /**< Mouse event, start feeding event by master */
    WIDGET_MOUSE_UNSET            = WIDGET_MOUSE_EVENT_WIDGET_MASK | WIDGET_MOUSE_EVENT_MASK | WIDGET_MOUSE_EVENT_UNSET, /**< Mouse event, stop feeding event by master */

    WIDGET_GBAR_MOUSE_ON_SCROLL   = WIDGET_MOUSE_EVENT_GBAR_MASK | WIDGET_MOUSE_EVENT_MASK | WIDGET_MOUSE_EVENT_ON_SCROLL, /**< Mouse event occurs while scrolling */
    WIDGET_GBAR_MOUSE_ON_HOLD     = WIDGET_MOUSE_EVENT_GBAR_MASK | WIDGET_MOUSE_EVENT_MASK | WIDGET_MOUSE_EVENT_ON_HOLD, /**< Mouse event occurs on holding */
    WIDGET_GBAR_MOUSE_OFF_SCROLL  = WIDGET_MOUSE_EVENT_GBAR_MASK | WIDGET_MOUSE_EVENT_MASK | WIDGET_MOUSE_EVENT_OFF_SCROLL, /**< Scrolling stopped */
    WIDGET_GBAR_MOUSE_OFF_HOLD    = WIDGET_MOUSE_EVENT_GBAR_MASK | WIDGET_MOUSE_EVENT_MASK | WIDGET_MOUSE_EVENT_OFF_HOLD, /**< Holding stopped */

    WIDGET_GBAR_MOUSE_DOWN        = WIDGET_MOUSE_EVENT_GBAR_MASK | WIDGET_MOUSE_EVENT_MASK | WIDGET_MOUSE_EVENT_DOWN, /**< Mouse down on the Glance Bar */
    WIDGET_GBAR_MOUSE_UP          = WIDGET_MOUSE_EVENT_GBAR_MASK | WIDGET_MOUSE_EVENT_MASK | WIDGET_MOUSE_EVENT_UP, /**< Mouse up on the Glance Bar */
    WIDGET_GBAR_MOUSE_MOVE        = WIDGET_MOUSE_EVENT_GBAR_MASK | WIDGET_MOUSE_EVENT_MASK | WIDGET_MOUSE_EVENT_MOVE, /**< Mouse move on the Glance Bar */
    WIDGET_GBAR_MOUSE_ENTER       = WIDGET_MOUSE_EVENT_GBAR_MASK | WIDGET_MOUSE_EVENT_MASK | WIDGET_MOUSE_EVENT_ENTER, /**< Mouse enter to the Glance Bar */
    WIDGET_GBAR_MOUSE_LEAVE       = WIDGET_MOUSE_EVENT_GBAR_MASK | WIDGET_MOUSE_EVENT_MASK | WIDGET_MOUSE_EVENT_LEAVE, /**< Mouse leave from the Glance Bar */
    WIDGET_GBAR_MOUSE_SET         = WIDGET_MOUSE_EVENT_GBAR_MASK | WIDGET_MOUSE_EVENT_MASK | WIDGET_MOUSE_EVENT_SET, /**< Mouse event, start feeding event by master */
    WIDGET_GBAR_MOUSE_UNSET       = WIDGET_MOUSE_EVENT_GBAR_MASK | WIDGET_MOUSE_EVENT_MASK | WIDGET_MOUSE_EVENT_UNSET, /**< Mouse event, stop feeding event by master */

    WIDGET_MOUSE_EVENT_MAX        = 0xFFFFFFFF /**< Unknown event */
} widget_mouse_event_type_e;

typedef enum widget_key_event_type {
    WIDGET_KEY_EVENT_MASK         = 0x80000000, /**< Mask value for key event */
    WIDGET_KEY_EVENT_GBAR_MASK    = 0x10000000, /**< Mask value for Glance Bar event */
    WIDGET_KEY_EVENT_WIDGET_MASK    = 0x40000000, /**< Mask value for widget event */

    WIDGET_KEY_EVENT_DOWN         = 0x00000001, /**< widget key press */
    WIDGET_KEY_EVENT_UP           = 0x00000002, /**< widget key release */
    WIDGET_KEY_EVENT_FOCUS_IN     = 0x00000008, /**< widget key focused in */
    WIDGET_KEY_EVENT_FOCUS_OUT    = 0x00000010, /**< widget key focused out */
    WIDGET_KEY_EVENT_SET          = 0x00000020, /**< widget Key, start feeding event by master */
    WIDGET_KEY_EVENT_UNSET        = 0x00000040, /**< widget key, stop feeding event by master */

    WIDGET_KEY_DOWN               = WIDGET_KEY_EVENT_MASK | WIDGET_KEY_EVENT_WIDGET_MASK | WIDGET_KEY_EVENT_DOWN, /**< Key down on the widget */
    WIDGET_KEY_UP                 = WIDGET_KEY_EVENT_MASK | WIDGET_KEY_EVENT_WIDGET_MASK | WIDGET_KEY_EVENT_UP, /**< Key up on the widget */
    WIDGET_KEY_SET                = WIDGET_KEY_EVENT_MASK | WIDGET_KEY_EVENT_WIDGET_MASK | WIDGET_KEY_EVENT_SET, /**< Key event, start feeding event by master */
    WIDGET_KEY_UNSET              = WIDGET_KEY_EVENT_MASK | WIDGET_KEY_EVENT_WIDGET_MASK | WIDGET_KEY_EVENT_UNSET, /**< Key event, stop feeding event by master */
    WIDGET_KEY_FOCUS_IN           = WIDGET_KEY_EVENT_MASK | WIDGET_KEY_EVENT_WIDGET_MASK | WIDGET_KEY_EVENT_FOCUS_IN, /**< Key event, focus in */
    WIDGET_KEY_FOCUS_OUT          = WIDGET_KEY_EVENT_MASK | WIDGET_KEY_EVENT_WIDGET_MASK | WIDGET_KEY_EVENT_FOCUS_OUT, /**< Key event, foucs out */
                                                               
    WIDGET_GBAR_KEY_DOWN          = WIDGET_KEY_EVENT_MASK | WIDGET_KEY_EVENT_GBAR_MASK | WIDGET_KEY_EVENT_DOWN, /**< Key down on the widget */
    WIDGET_GBAR_KEY_UP            = WIDGET_KEY_EVENT_MASK | WIDGET_KEY_EVENT_GBAR_MASK | WIDGET_KEY_EVENT_UP, /**< Key up on the widget */
    WIDGET_GBAR_KEY_SET           = WIDGET_KEY_EVENT_MASK | WIDGET_KEY_EVENT_GBAR_MASK | WIDGET_KEY_EVENT_SET, /**< Key event, start feeding event by master */
    WIDGET_GBAR_KEY_UNSET         = WIDGET_KEY_EVENT_MASK | WIDGET_KEY_EVENT_GBAR_MASK | WIDGET_KEY_EVENT_UNSET, /**< Key event, stop feeding event by master */
    WIDGET_GBAR_KEY_FOCUS_IN      = WIDGET_KEY_EVENT_MASK | WIDGET_KEY_EVENT_GBAR_MASK | WIDGET_KEY_EVENT_FOCUS_IN, /**< Key event, focus in */
    WIDGET_GBAR_KEY_FOCUS_OUT     = WIDGET_KEY_EVENT_MASK | WIDGET_KEY_EVENT_GBAR_MASK | WIDGET_KEY_EVENT_FOCUS_OUT, /**< Key event, focus out */

    WIDGET_KEY_EVENT_MAX          = 0xFFFFFFFF /**< Unknown event */
} widget_key_event_type_e;

/**
 * @internal
 * @brief Enumeration for Accessibility event for buffer type widget or Glance Bar.
 * @details These events are sync'd with Tizen accessibility event set.
 * @since_tizen 2.3.1
 */
typedef enum widget_access_event_type {
    WIDGET_ACCESS_EVENT_GBAR_MASK    = 0x10000000, /**< Glance Bar Accessibilivent mask */
    WIDGET_ACCESS_EVENT_WIDGET_MASK    = 0x20000000, /**< widget Accessibility event mask */

    WIDGET_ACCESS_EVENT_HIGHLIGHT    = 0x00000100, /**< widget accessibility: Hightlight a object, Next, Prev,Unhighlight */
    WIDGET_ACCESS_EVENT_ACTIVATE     = 0x00000200, /**< widget accessibility activate */
    WIDGET_ACCESS_EVENT_ACTION       = 0x00000400, /**< widget accessibility value changed, Up, Down */
    WIDGET_ACCESS_EVENT_SCROLL       = 0x00000800, /**< widget accessibility scroll down, move, up */
    WIDGET_ACCESS_EVENT_VALUE_CHANGE = 0x00001000, /**< LB accessibility value change */
    WIDGET_ACCESS_EVENT_MOUSE        = 0x00002000, /**< Give mouse event to highlight object, down, move, up */
    WIDGET_ACCESS_EVENT_BACK         = 0x00004000, /**< Go back to a previous view ex: pop naviframe item */
    WIDGET_ACCESS_EVENT_OVER         = 0x00008000, /**< Mouse over an object */
    WIDGET_ACCESS_EVENT_READ         = 0x00010000, /**< Highlight an object */
    WIDGET_ACCESS_EVENT_ENABLE       = 0x00020000, /**< Disable highlight and read ability, disable, enable */

    WIDGET_ACCESS_HIGHLIGHT          = WIDGET_ACCESS_EVENT_WIDGET_MASK | WIDGET_ACCESS_EVENT_HIGHLIGHT, /**< Access event - Highlight an object in the widget */
    WIDGET_ACCESS_ACTIVATE           = WIDGET_ACCESS_EVENT_WIDGET_MASK | WIDGET_ACCESS_EVENT_ACTIVATE,  /**< Access event - Launch or activate the highlighted object */
    WIDGET_ACCESS_ACTION             = WIDGET_ACCESS_EVENT_WIDGET_MASK | WIDGET_ACCESS_EVENT_ACTION,    /**< Access event - down */
    WIDGET_ACCESS_SCROLL             = WIDGET_ACCESS_EVENT_WIDGET_MASK | WIDGET_ACCESS_EVENT_SCROLL,    /**< Access event - scroll down */
    WIDGET_ACCESS_VALUE_CHANGE       = WIDGET_ACCESS_EVENT_WIDGET_MASK | WIDGET_ACCESS_EVENT_VALUE_CHANGE, /**< LB accessibility value change */
    WIDGET_ACCESS_MOUSE              = WIDGET_ACCESS_EVENT_WIDGET_MASK | WIDGET_ACCESS_EVENT_MOUSE,  /**< Give mouse event to highlight object */
    WIDGET_ACCESS_BACK               = WIDGET_ACCESS_EVENT_WIDGET_MASK | WIDGET_ACCESS_EVENT_BACK,   /**< Go back to a previous view ex: pop naviframe item */
    WIDGET_ACCESS_OVER               = WIDGET_ACCESS_EVENT_WIDGET_MASK | WIDGET_ACCESS_EVENT_OVER,   /**< Mouse over an object */
    WIDGET_ACCESS_READ               = WIDGET_ACCESS_EVENT_WIDGET_MASK | WIDGET_ACCESS_EVENT_READ,   /**< Highlight an object */
    WIDGET_ACCESS_ENABLE             = WIDGET_ACCESS_EVENT_WIDGET_MASK | WIDGET_ACCESS_EVENT_ENABLE, /**< Enable highlight and read ability */

    WIDGET_GBAR_ACCESS_HIGHLIGHT     = WIDGET_ACCESS_EVENT_GBAR_MASK | WIDGET_ACCESS_EVENT_HIGHLIGHT, /**< Access event - Highlight an object in the Glance Bar */
    WIDGET_GBAR_ACCESS_ACTIVATE      = WIDGET_ACCESS_EVENT_GBAR_MASK | WIDGET_ACCESS_EVENT_ACTIVATE,  /**< Access event - Launch or activate the highlighted object */
    WIDGET_GBAR_ACCESS_ACTION        = WIDGET_ACCESS_EVENT_GBAR_MASK | WIDGET_ACCESS_EVENT_ACTION,    /**< Access event - down */
    WIDGET_GBAR_ACCESS_SCROLL        = WIDGET_ACCESS_EVENT_GBAR_MASK | WIDGET_ACCESS_EVENT_SCROLL,    /**< Access event - scroll down */
    WIDGET_GBAR_ACCESS_VALUE_CHANGE  = WIDGET_ACCESS_EVENT_GBAR_MASK | WIDGET_ACCESS_EVENT_VALUE_CHANGE, /**< LB accessibility value change */
    WIDGET_GBAR_ACCESS_MOUSE         = WIDGET_ACCESS_EVENT_GBAR_MASK | WIDGET_ACCESS_EVENT_MOUSE, /**< Give mouse event to highlight object */
    WIDGET_GBAR_ACCESS_BACK          = WIDGET_ACCESS_EVENT_GBAR_MASK | WIDGET_ACCESS_EVENT_BACK, /**< Go back to a previous view ex: pop naviframe item */
    WIDGET_GBAR_ACCESS_OVER          = WIDGET_ACCESS_EVENT_GBAR_MASK | WIDGET_ACCESS_EVENT_OVER, /**< Mouse over an object */
    WIDGET_GBAR_ACCESS_READ          = WIDGET_ACCESS_EVENT_GBAR_MASK | WIDGET_ACCESS_EVENT_READ, /**< Highlight an object */
    WIDGET_GBAR_ACCESS_ENABLE        = WIDGET_ACCESS_EVENT_GBAR_MASK | WIDGET_ACCESS_EVENT_ENABLE, /**< Enable highlight and read ability */
    WIDGET_GBAR_ACCESS_EVENT_MAX     = 0xFFFFFFFF
} widget_access_event_type_e;

/**
 * @internal
 * @brief Enumeration for widget content type.
 * @since_tizen 2.3.1
 */
typedef enum widget_type {
    WIDGET_CONTENT_TYPE_IMAGE       = 0x01,       /**< Contents of a widget is based on the image file */
    WIDGET_CONTENT_TYPE_BUFFER      = 0x02,       /**< Contents of a widget is based on canvas buffer(shared) */
    WIDGET_CONTENT_TYPE_TEXT        = 0x04,       /**< Contents of a widget is based on formatted text file */
    WIDGET_CONTENT_TYPE_RESOURCE_ID = 0x08,       /**< Contens of a widget is shared by the resource id(depends on window system) */
    WIDGET_CONTENT_TYPE_UIFW        = 0x10,       /**< Using UI F/W resource for sharing content & event */
    WIDGET_CONTENT_TYPE_INVALID     = 0xFF        /**< Unknown widget type */
} widget_type_e;

/**
 * @brief Enumeration for widget option types.
 * @since_tizen 2.3.1
 */
typedef enum widget_option_type {
    WIDGET_OPTION_MANUAL_SYNC,           /**< Sync frame manually */
    WIDGET_OPTION_FRAME_DROP_FOR_RESIZE, /**< Drop frames while resizing */
    WIDGET_OPTION_SHARED_CONTENT,        /**< Use only one real instance for multiple fake instances if user creates widget for same content */
    WIDGET_OPTION_DIRECT_UPDATE,         /**< Use the private socket for receiving updated event */
    WIDGET_OPTION_EXTRA_BUFFER_CNT,      /**< Extra buffer count, ReadOnly value */    

    WIDGET_OPTION_ERROR = 0xFFFFFFFF     /**< To specify the size of this enumeration type */
} widget_option_type_e;

/**
 * @internal
 * @brief Reason of faults
 * @since_tizen 2.3.1
 */
typedef enum widget_fault_type {
    WIDGET_FAULT_DEACTIVATED,                     /**< widget is deactivated by its fault operation */
    WIDGET_FAULT_PROVIDER_DISCONNECTED,           /**< Provider is disconnected */
    WIDGET_FAULT_MAX = 0xFF                       /**< To specify the size of this enumeration type, some compiler enjoy of this kind of notation */
} widget_fault_type_e;

/**
 * @brief Enumeration for widget visible states.
 * @details Must be sync'd with a provider.
 * @since_tizen 2.3.1
 */
typedef enum widget_visible_state {
    WIDGET_SHOW            = 0x00,                /**< widget is shown. Default state */
    WIDGET_HIDE            = 0x01,                /**< widget is hidden, Update timer will not be freezed. but you cannot receive any updates events. */

    WIDGET_HIDE_WITH_PAUSE = 0x02,                /**< widget is hidden, it will pause the update timer, but if a widget updates its contents, update event will be triggered */

    WIDGET_VISIBLE_ERROR   = 0xFF                 /**< To specify the size of this enumeration type */
} widget_visible_state_e;

/**
 * @internal
 * @brief Accessibility Event type
 * @since_tizen 2.3.1
 * @see widget_feed_access_event()
 */
typedef enum widget_access_info_type {
    WIDGET_ACCESS_TYPE_NONE = 0x00,           /**< Initialized */

    WIDGET_ACCESS_TYPE_DOWN = 0x00,           /**< Mouse down */
    WIDGET_ACCESS_TYPE_MOVE = 0x01,           /**< Mouse move */
    WIDGET_ACCESS_TYPE_UP   = 0x02,           /**< Mouse up */

    WIDGET_ACCESS_TYPE_HIGHLIGHT      = 0x00, /**< Highlight */
    WIDGET_ACCESS_TYPE_HIGHLIGHT_NEXT = 0x01, /**< Highlight next */
    WIDGET_ACCESS_TYPE_HIGHLIGHT_PREV = 0x02, /**< Highlight prev */
    WIDGET_ACCESS_TYPE_UNHIGHLIGHT    = 0x03, /**< Unhighlight */

    WIDGET_ACCESS_TYPE_DISABLE = 0x00,        /**< Disable */
    WIDGET_ACCESS_TYPE_ENABLE  = 0x01         /**< Enable */
} widget_access_info_type_e;

/**
 * @internal
 * @brief Accessibility Event Information
 * @since_tizen 2.3.1
 */
typedef struct widget_access_event_info {
    double x;                                   /**< X Coordinates that the event occurred */
    double y;                                   /**< Y Coordinates that the event occurred */
    widget_access_info_type_e type;         /**< Accessibility event type */
    int info;                                   /**< Extra information for this event */
} *widget_access_event_info_s;

/**
 * @internal
 * @brief Damaged Region representation
 * @since_tizen 2.3.1
 */
typedef struct widget_damage_region {
    int x;                                  /**< Coordinates X of Left-Top corner */
    int y;                                  /**< Coordinates Y of Left-Top corner */
    int w;                                  /**< Damage'd Width */
    int h;                                  /**< Damage'd Height */
} widget_damage_region_s;

/**
 * @internal
 * @brief Mouse Event Information
 * @since_tizen 2.3.1
 */
typedef struct widget_mouse_event_info {
	double x;                                   /**< X coordinates of Mouse Event */
	double y;                                   /**< Y coordinates of Mouse Event */
	double ratio_w;
	double ratio_h;
	int device;
} *widget_mouse_event_info_s;

/**
 * @internal
 * @brief Key Event Information
 * @since_tizen 2.3.1
 */
typedef struct widget_key_event_info {
    unsigned int keycode;                       /**< Key code */
	int device;
} *widget_key_event_info_s;

/**
 * @internal
 * @brief Structure for TEXT type widget contents handling opertators.
 * @since_tizen 2.3.1
 */
typedef struct widget_script_operators {
    int (*update_begin)(widget_h handle); /**< Content parser is started */
    int (*update_end)(widget_h handle); /**< Content parser is finished */

    /* Listed functions will be called when parser meets each typed content */
    int (*update_text)(widget_h handle, const char *id, const char *part, const char *data); /**< Update text content */
    int (*update_image)(widget_h handle, const char *id, const char *part, const char *data, const char *option); /**< Update image content */
    int (*update_script)(widget_h handle, const char *id, const char *new_id, const char *part, const char *file, const char *group); /**< Update script content */
    int (*update_signal)(widget_h handle, const char *id, const char *signal_name, const char *signal); /**< Update signal */
    int (*update_drag)(widget_h handle, const char *id, const char *part, double dx, double dy); /**< Update drag info */
    int (*update_info_size)(widget_h handle, const char *id, int w, int h); /**< Update content size */
    int (*update_info_category)(widget_h handle, const char *id, const char *category); /**< Update content category info */
    int (*update_access)(widget_h handle, const char *id, const char *part, const char *text, const char *option); /**< Update access information */
    int (*operate_access)(widget_h handle, const char *id, const char *part, const char *operation, const char *option); /**< Update access operation */
    int (*update_color)(widget_h handle, const char *id, const char *part, const char *data); /**< Update color */
} *widget_script_operator_s;

/**
 * @internal
 * @brief Called for every async function.
 * @details Prototype of the return callback of every async functions.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of the widget instance
 * @param[in] ret Result status of operation (WIDGET_STATUS_XXX defined from libwidget-service)
 * @param[in] data Data for result callback
 * @see widget_add()
 * @see widget_del()
 * @see widget_activate()
 * @see widget_resize()
 * @see widget_set_group()
 * @see widget_set_period()
 * @see widget_access_event()
 * @see widget_set_pinup()
 * @see widget_create_glance_bar()
 * @see widget_destroy_glance_bar()
 * @see widget_emit_text_signal()
 * @see widget_acquire_resource_id()
 * @see widget_set_update_mode()
 */
typedef void (*widget_ret_cb)(widget_h handle, int ret, void *data);

/**
 * @internal
 * @brief Fault event handle
 * @param[in] type Type of fault event.
 * @param[in] widget_id Faulted widget Id
 * @param[in] file faulted filename (implementation file if it is supported)
 * @param[in] func faulted function name (if it is supported)
 * @param[in] data Callback data
 * @return 0 on success, otherwise a negative error value
 * @retval @c EXIT_FAILURE delete this event callback from the event callback list
 * @retval @c EXIT_SUCCESS successfully handled, keep this callback in the event callback list
 */
typedef int (*widget_fault_handler_cb)(enum widget_fault_type type, const char *widget_id, const char *file, const char *func, void *data);

/**
 * @brief Event handle
 * @since_tizen 2.3.1
 * @param[in] handle widget Event handle
 * @param[in] event Event type for widget
 * @param[in] data Callback Data
 * @return 0 on success, otherwise a negative error value
 * @return @c EXIT_FAILURE delete this event callback from the event callback list
 * @return @c EXIT_SUCCESS successfully handled, keep this callback in the event callback list
 */
typedef int (*widget_event_handler_cb)(widget_h handle, widget_event_type_e event, void *data);

/**
 * @brief Auto launch handle
 * @since_tizen 2.3.1
 * @param[in] handle widget Handle
 * @param[in] appid UI Application Id, which should be launched
 * @param[in] data callback data
 */
typedef int (*widget_auto_launch_handler_cb)(widget_h handle, const char *appid, void *data);

/**
 * @internal
 * @brief Initializes the widget system with some options.
 * @details widget_init function uses environment value to initiate some configurable values.
 *          But some applications do not want to use the env value.
 *          For them, this API will give a chance to set default options using given arguments.
 *          @a disp is a Display object which is used to hold a connection with a display server (eg, Xorg)
 * @since_tizen 2.3.1
 * @param[in] disp Display, If @a disp is @c NULL, the library will try to acquire a new connection to display server
 * @param[in] prevent_overwrite Overwrite flag (when the content of an image type widget is updated, it will be overwriten (0) or not (1))
 * @param[in] event_filter If the widget_feed_mouse_event() is called again in this secs, it will be ignored and the widget_feed_mouse_event() will returns WIDGET_STATUS_ERROR_BUSY status code
 * @param[in] use_thread If this value has true, the viewer library will create a new thread to communicate with master service
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return #WIDGET_STATUS_ERROR_NONE on success,
 *          otherwise an error code (see #WIDGET_STATUS_ERROR_XXX) on failure
 * @retval #WIDGET_STATUS_ERROR_OUT_OF_MEMORY If a memory is not enough to do this operation.
 * @retval #WIDGET_STATUS_ERROR_IO_ERROR If fails to access widget database.
 * @see widget_fini()
 * @see widget_feed_mouse_event()
 */
extern int widget_viewer_init(void *disp, int prevent_overwrite, double event_filter, int use_thread);

/**
 * @internal
 * @brief Finalizes the widget system.
 * @since_tizen 2.3.1
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_SUCCES if success
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER if widget_init is not called
 * @see widget_init()
 */
extern int widget_viewer_fini(void);

/**
 * @brief Notifies the status of a client ("it is paused") to the provider.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return #WIDGET_STATUS_ERROR_NONE on success,
 *          otherwise an error code (see #WIDGET_STATUS_ERROR_XXX) on failure
 * @retval #WIDGET_STATUS_ERROR_FAULT if it failed to send state (paused) info
 * @see widget_client_set_resumed()
 */
extern int widget_viewer_notify_paused_status_of_viewer(void);

/**
 * @brief Notifies the status of client ("it is resumed") to the provider.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return #WIDGET_STATUS_ERROR_NONE on success,
 *          otherwise an error code (see #WIDGET_STATUS_ERROR_XXX) on failure
 * @retval #WIDGET_STATUS_ERROR_FAULT if it failed to send state (resumed) info
 * @see widget_client_set_paused()
 */
extern int widget_viewer_notify_resumed_status_of_viewer(void);

/**
 * @internal
 * @brief Adds a new widget.
 * @details If the screen size is "1280x720", the below size lists are used for default.
 * Or you can find the default sizes in pixel from /usr/share/data-provider-master/resolution.ini.
 * Size types are defined from the libwidget-service package (widget-service.h).
 *
 * Normal mode widget
 * 1x1=175x175, #WIDGET_SIZE_TYPE_1x1
 * 2x1=354x175, #WIDGET_SIZE_TYPE_2x1
 * 2x2=354x354, #WIDGET_SIZE_TYPE_2x2
 * 4x1=712x175, #WIDGET_SIZE_TYPE_4x1
 * 4x2=712x354, #WIDGET_SIZE_TYPE_4x2
 * 4x4=712x712, #WIDGET_SIZE_TYPE_4x4
 *
 * Extended sizes
 * 4x3=712x533, #WIDGET_SIZE_TYPE_4x3
 * 4x5=712x891, #WIDGET_SIZE_TYPE_4x5
 * 4x6=712x1070, #WIDGET_SIZE_TYPE_4x6
 *
 * Easy mode widget
 * 21x21=224x215, #WIDGET_SIZE_TYPE_EASY_1x1
 * 23x21=680x215, #WIDGET_SIZE_TYPE_EASY_3x1
 * 23x23=680x653, #WIDGET_SIZE_TYPE_EASY_3x3
 *
 * Special widget
 * 0x0=720x1280, #WIDGET_SIZE_TYPE_FULL
 * @since_tizen 2.3.1
 * @remarks
 *    This is an ASYNCHRONOUS API.
 *    Even if you get a handle from the return value of this function, it is not a created instance.
 *    So you have to consider it as a not initialized handle.
 *    It can be initialized only after getting the return callback with "ret == #WIDGET_STATUS_ERROR_NONE"
 *    This function is Asynchronous, so you will get result of add requst from @a cb, if you failed to send request to create a new widget,
 *    This function will returns proper error code
 *    If this returns @c NULL, you can get the reason of failure using get_last_result()
 * @param[in] widget_id widget Id
 * @param[in] content Contents that will be given to the widget instance
 * @param[in] cluster Identifier to group widgets which will be decided in runtime.
 * @param[in] category Identifier to group widgets declared in the manifest file by the widget application.
 * @param[in] period Update period (@c WIDGET_DEFAULT_PERIOD can be used for this; this argument will be used to specify the period of updating contents of a widget)
 * @param[in] type Size type (defined from libwidget-service package)
 * @param[in] cb After the request is sent to the master provider, this callback will be called
 * @param[in] data This data will be passed to the callback
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return handle
 * @retval Handle widget handle but not yet initialized
 * @retval @c NULL if it fails to create a handle
 * @see widget_ret_cb
 */
extern widget_h widget_viewer_add_widget(const char *widget_id, const char *content, const char *cluster, const char *category, double period, widget_size_type_e type, widget_ret_cb cb, void *data);

/**
 * @internal
 * @brief Deletes a widget (will replace widget_del).
 * @since_tizen 2.3.1
 * @remarks
 *    This is an ASYNCHRONOUS API.
 *    If you call this with an uninitialized handle, the return callback will be called synchronously.
 *    So before returning from this function, the return callback will be called first.
 *    This function is Asynchronous, so you will get result of add requst from @a cb, if you failed to send request to create a new widget,
 *    This function will returns proper error code
 * @param[in] handle Handle of a widget instance
 * @param[in] type Deletion type (WIDGET_DELETE_PERMANENTLY or WIDGET_DELETE_TEMPORARY)
 * @param[in] cb Return callback
 * @param[in] data User data for return callback
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_BUSY Already in process
 * @retval #WIDGET_STATUS_ERROR_FAULT Failed to create a request packet
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully sent, return callack will be called
 * @see widget_ret_cb
 */
extern int widget_viewer_delete_widget(widget_h handle, widget_delete_type_e type, widget_ret_cb cb, void *data);

/**
 * @internal
 * @brief Sets a widget events callback.
 * @details To get the event which is pushed from the provider, Register the event callback using this API.
 *    The registered callback will be invoked if there are any events from the provider.
 * @since_tizen 2.3.1
 * @param[in] cb Event handle
 * @param[in] data User data for the event handle
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_NONE If succeed to set event handle
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_OUT_OF_MEMORY Not enough memory
 * @see widget_unset_event_handler()
 */
extern int widget_viewer_add_event_handler(widget_event_handler_cb cb, void *data);

/**
 * @brief Unsets the widget event handle.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] cb Event handle
 * @retval pointer Pointer of 'data' which is used with the widget_set_event_handler
 * @see widget_set_event_handler()
 */
extern void *widget_viewer_remove_event_handler(widget_event_handler_cb cb);

/**
 * @internal
 * @brief Registers the widget fault event handle.
 * @details Argument list: event, pkgname, filename, funcname.
 * @since_tizen 2.3.1
 * @param[in] cb Event handle
 * @param[in] data Event handle data
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_NONE If succeed to set fault event handle
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_OUT_OF_MEMORY Not enough memory
 * @see widget_unset_fault_handler()
 */
extern int widget_viewer_add_fault_handler(widget_fault_handler_cb cb, void *data);

/**
 * @brief Unsets the widget fault event handle.
 * @since_tizen 2.3.1
 * * @param[in] cb Event handle
 * @retval pointer Pointer of 'data' which is used with the widget_set_fault_handler
 * @see widget_set_fault_handler()
 */
extern void *widget_viewer_remove_fault_handler(widget_fault_handler_cb cb);

/**
 * @internal
 * @brief Activates the faulted widget.
 * @details Request result will be returned via return callback.
 * @since_tizen 2.3.1
 * @remarks
 *    This is an ASYNCHRONOUS API.
 *    Even though this function returns ERROR_NONE, it means that it just successfully sent a request to the provider.
 *    So you have to check the return callback and its "ret" argument.
 *    This function is Asynchronous, so you will get result of add requst from @a cb, if you failed to send request to create a new widget,
 *    This function will returns proper error code
 * @param[in] widget_id Package name which should be activated
 * @param[in] cb Result callback
 * @param[in] data Callback data
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return #WIDGET_STATUS_ERROR_NONE on success,
 *          otherwise an error code (see #WIDGET_STATUS_ERROR_XXX) on failure
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully sent a request
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_FAULT Failed to make a request
 * @see widget_ret_cb
 */
extern int widget_viewer_activate_faulted_widget(const char *widget_id, widget_ret_cb cb, void *data);

/**
 * @internal
 * @brief Resizes the widget.
 * @details
 * Normal mode widget size
 * 1x1=175x175, WIDGET_SIZE_TYPE_1x1
 * 2x1=354x175, WIDGET_SIZE_TYPE_2x1
 * 2x2=354x354, WIDGET_SIZE_TYPE_2x2
 * 4x1=712x175, WIDGET_SIZE_TYPE_4x1
 * 4x2=712x354, WIDGET_SIZE_TYPE_4x2
 * 4x4=712x712, WIDGET_SIZE_TYPE_4x4
 *
 * Extended widget size
 * 4x3=712x533, WIDGET_SIZE_TYPE_4x3
 * 4x5=712x891, WIDGET_SIZE_TYPE_4x5
 * 4x6=712x1070, WIDGET_SIZE_TYPE_4x6
 *
 * Easy mode widget size
 * 21x21=224x215, WIDGET_SIZE_TYPE_EASY_1x1
 * 23x21=680x215, WIDGET_SIZE_TYPE_EASY_3x1
 * 23x23=680x653, WIDGET_SIZE_TYPE_EASY_3x3
 *
 * Special mode widget size
 * 0x0=720x1280, WIDGET_SIZE_TYPE_FULL
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @remarks
 *    This is an ASYNCHRONOUS API.
 *    This function is Asynchronous, so you will get result of add requst from @a cb, if you failed to send request to create a new widget,
 *    This function will returns proper error code
 * @param[in] handle Handle of a widget instance
 * @param[in] type Type of a widget size (e.g., WIDGET_SIZE_TYPE_1x1, ...)
 * @param[in] cb Result callback of the resize operation
 * @param[in] data User data for return callback
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return #WIDGET_STATUS_ERROR_NONE on success,
 *          otherwise an error code (see #WIDGET_STATUS_ERROR_XXX) on failure
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_BUSY Previous request of resize is in progress
 * @retval #WIDGET_STATUS_ERROR_ALREADY Already resized, there is no differences between current size and requested size
 * @retval #WIDGET_STATUS_ERROR_PERMISSION_DENIED Permission denied, you only have view the content of this box
 * @retval #WIDGET_STATUS_ERROR_FAULT Failed to make a request
 * @see widget_ret_cb
 */
extern int widget_viewer_resize_widget(widget_h handle, widget_size_type_e type, widget_ret_cb cb, void *data);

/**
 * @internal
 * @brief Sends the click event to a widget, This is not related with mouse_event, viewer can send "clicked" event directly.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @param[in] x Rational X of the content width
 * @param[in] y Rational Y of the content height
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully done
 */
extern int widget_viewer_send_click_event(widget_h handle, const char *event, double x, double y);

/**
 * @internal
 * @brief Changes the cluster/sub-cluster name of the given widget handle.
 * @since_tizen 2.3.1
 * @remarks
 *    This is an ASYNCHRONOUS API.
 *    This function is Asynchronous, so you will get result of add requst from @a cb, if you failed to send request to create a new widget,
 *    This function will returns proper error code
 * @param[in] handle Handle of a widget instance
 * @param[in] cluster New cluster of a widget
 * @param[in] category New category of a widget
 * @param[in] cb Result callback for changing the cluster/category of a widget
 * @param[in] data User data for the result callback
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_NONE Request is successfully sent. the return callback will be called
 * @retval #WIDGET_STATUS_ERROR_BUSY Previous request is not finished yet
 * @retval #WIDGET_STATUS_ERROR_ALREADY Group name is same with current one
 * @retval #WIDGET_STATUS_ERROR_PERMISSION_DENIED You have no permission to change property of this widget instance
 * @retval #WIDGET_STATUS_ERROR_FAULT Failed to make a request
 * @see widget_ret_cb
 */
extern int widget_viewer_set_group(widget_h handle, const char *cluster, const char *category, widget_ret_cb cb, void *data);

/**
 * @internal
 * @brief Gets the cluster and category (sub-cluster) name of the given widget (it is not I18N format, only English).
 * @since_tizen 2.3.1
 * @remarks You have to do not release the cluster & category.
 *    It is allocated inside of a given widget instance, so you can only read it.
 * @param[in] handle Handle of a widget instance
 * @param[out] cluster Storage(memory) for containing the cluster name
 * @param[out] category Storage(memory) for containing the category name
 * @return #WIDGET_STATUS_ERROR_NONE on success,
 *          otherwise an error code (see #WIDGET_STATUS_ERROR_XXX) on failure
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully done
 */
extern int widget_viewer_get_group(widget_h handle, const char **cluster, const char **category);

/**
 * @brief Gets the update period of the widget.
 * @since_tizen 2.3.1
 * * @param[in] handle Handle of a widget instance
 * @param[out] period Update period of the widget.
 * @return #WIDGET_STATUS_ERROR_NONE on success,
 *          otherwise an error code (see #WIDGET_STATUS_ERROR_XXX) on failure
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid parameters
 */
extern int widget_viewer_get_period(widget_h handle, double *period);

/**
 * @internal
 * @brief Changes the update period.
 * @since_tizen 2.3.1
 * @remarks
 *    This is an ASYNCHRONOUS API.
 *    This function is Asynchronous, so you will get result of add requst from @a cb, if you failed to send request to create a new widget,
 *    This function will returns proper error code
 * @param[in] handle Handle of a widget instance
 * @param[in] period New update period of a widget
 * @param[in] cb Result callback of changing the update period of this widget
 * @param[in] data User data for the result callback
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully done
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_BUSY
 * @retval #WIDGET_STATUS_ERROR_ALREADY
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @see widget_ret_cb
 */
extern int widget_viewer_set_period(widget_h handle, double period, widget_ret_cb cb, void *data);

/**
 * @brief Checks whether the given widget is a text type or not.
 * @remarks
 *    If this returns WIDGET_CONTENT_TYPE_INVALID, you can get the reason of failure using get_last_result()
 * @since_tizen 2.3.1
 * * @param[in] handle Handle of a widget instance
 * @param[in] gbar 1 for Glance Bar or 0
 * @return widget_type
 * @retval #WIDGET_CONTENT_TYPE_IMAGE Contents of a widget is based on the image file
 * @retval #WIDGET_CONTENT_TYPE_BUFFER Contents of a widget is based on canvas buffer(shared)
 * @retval #WIDGET_CONTENT_TYPE_TEXT Contents of a widget is based on formatted text file
 * @retval #WIDGET_CONTENT_TYPE_RESOURCE_ID Contens of a widget is shared by the resource id (depends on the Window system, eg, Xorg)
 * @retval #WIDGET_CONTENT_TYPE_UIFW UI F/W supported content type for widget
 * @retval #WIDGET_CONTENT_TYPE_INVALID Invalid type
 * @see widget_type()
 */
extern int widget_viewer_get_type(widget_h handle, int gbar, widget_type_e *widget_type);

/**
 * @brief Checks if the given widget is created by user or not.
 * @remarks if this returns negative value, you can get the reason of failure using get_last_result()
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @details If the widget instance is created by a system this will return 0.
 * @param[in] handle Handle of a widget instance
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval 0 Automatically created widget by the provider
 * @retval 1 Created by user via widget_add()
 * @see widget_add()
 * @see widget_set_event_handler()
 */
extern int widget_viewer_is_created_by_user(widget_h handle);

/**
 * @internal
 * @brief Gets content information string of the given widget.
 * @remarks if this returns @c NULL, you can get the reason of failure using get_last_result()
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @return const char *
 * @retval content_info widget content info that can be used again via content_info argument of widget_add()
 * @see widget_add()
 */
extern const char *widget_viewer_get_content_string(widget_h handle);

/**
 * @brief Gets the sub cluster title string of the given widget.
 * @details This API is now used for accessibility.
 *  Each box should set their content as a string to be read by TTS.
 *  So if the box has focused on the homescreen, the homescreen will read text using this API.
 * @since_tizen 2.3.1
 * * @remarks The title returned by this API can be read by TTS.
 *  But it is just recomendation for the homescreen.
 *  So, to read it or not depends on its implementation.
 *  if this returns @c NULL, you can get the reason of failure using get_last_result()
 * @param[in] handle Handle of a widget instance
 * @return const char *
 * @retval sub Cluster name
 * @retval @c NULL
 */
extern const char *widget_viewer_get_title_string(widget_h handle);

/**
 * @internal
 * @brief Gets the filename of the given widget, if it is an IMAGE type widget.
 * @details If the box is developed as an image format to represent its contents, the homescreen should know its image file name.
 * @remarks if this returns @c NULL, you can get the reason of failure using get_last_result()
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @return const char *
 * @retval filename If the widget type is image this function will give you a abs-path of an image file (content is rendered)
 * @retval @c NULL If this has no image file or type is not image file.
 */
extern const char *widget_viewer_get_filename(widget_h handle);

/**
 * @brief Gets the package name of the given widget handle.
 * @remarks if this returns @c NULL, you can get the reason of failure using get_last_result()
 * @since_tizen 2.3.1
 * * @param[in] handle Handle of a widget instance
 * @return const char *
 * @retval pkgname Package name
 * @retval @c NULL If the handle is not valid
 */
extern const char *widget_viewer_get_pkgname(widget_h handle);

/**
 * @brief Gets the priority of a current content.
 * @remarks if this returns negative value, you can get the reason of failure using get_last_result()
 * @since_tizen 2.3.1
 * * @param[in] handle Handle of a widget instance
 * @param[out] priority priority of the widget
 * @return #WIDGET_STATUS_ERROR_NONE on success,
 *          otherwise an error code (see #WIDGET_STATUS_ERROR_XXX) on failure
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid parameters
 */
extern int widget_viewer_get_priority(widget_h handle, double *priority);

/**
 * @internal
 * @brief Acquires the buffer of a given widget (only for the buffer type).
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @param[in] gbar 1 for Glance Bar or 0
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @retval address Address of a Frame Buffer
 * @retval @c NULL If it fails to get buffer address
 */
extern void *widget_viewer_acquire_buffer(widget_h handle, int gbar);

/**
 * @brief Releases the buffer of a widget (only for the buffer type).
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] buffer Buffer
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully done
 * @see widget_acquire_buffer()
 */
extern int widget_viewer_release_buffer(void *buffer);

/**
 * @internal
 * @brief Gets the reference count of widget buffer (only for the buffer type).
 * @since_tizen 2.3.1
 * @param[in] buffer Buffer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @retval refcnt Positive integer value including ZERO
 */
extern int widget_viewer_get_buffer_reference_count(void *buffer);

/**
 * @brief Gets the size of the widget.
 * @remarks
 *   If this returns WIDGET_SIZE_TYPE_UNKNOWN, you can get the reason of failure using get_last_result()
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] handle Handle of a widget instance
 * @return widget_size_type_e
 * @retval #WIDGET_SIZE_TYPE_NxM N by M size
 * @retval #WIDGET_SIZE_TYPE_UNKNOWN Invalid handle or size type is not defined yet
 */
extern int widget_viewer_get_size_type(widget_h handle, widget_size_type_e *size_type);

/**
 * @internal
 * @brief Gets the size of the Glance Bar.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @param[out] w Width of glance bar in pixels
 * @param[out] h Height of glance bar in pixels
 * @return #WIDGET_STATUS_ERROR_NONE on success,
 *          otherwise an error code (see #WIDGET_STATUS_ERROR_XXX) on failure
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid parameters are used
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully done
 */
extern int widget_viewer_get_glance_bar_size(widget_h handle, int *w, int *h);

/**
 * @internal
 * @brief Gets a list of the supported sizes of a given handle.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @param[in] size_list Array buffer for getting the size types
 * @param[in] cnt size of array
 * @param[out] cnt Count of returned size types
 * @param[out] size_list Array of size types
 * @return #WIDGET_STATUS_ERROR_NONE on success,
 *          otherwise an error code (see #WIDGET_STATUS_ERROR_XXX) on failure
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully done
 */
extern int widget_viewer_get_supported_sizes(widget_h handle, int *cnt, widget_size_type_e *size_list);

/**
 * @internal
 * @brief Gets BUFFER SIZE of the widget if it is a buffer type.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @param[in] gbar 1 for Glance Bar or 0
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval size Size in bytes of the widget buffer
 */
extern int widget_viewer_get_buffer_size(widget_h handle, int gbar);

/**
 * @internal
 * @brief Sends a content event (for buffer type) to the provider (widget).
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @param[in] type Event type
 * @param[in] x Coordinates of X axis
 * @param[in] y Coordinates of Y axis
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_BUSY Previous operation is not finished yet
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully sent
 * @see widget_feed_access_event()
 * @see widget_feed_key_event()
 */
extern int widget_viewer_feed_mouse_event(widget_h handle, widget_mouse_event_type_e type, widget_mouse_event_info_s info);

/**
 * @internal
 * @brief Sends an access event (for buffer type) to the provider (widget).
 * @remarks
 *    This is an ASYNCHRONOUS API.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @param[in] type Event type
 * @param[in] x Coordinates of X axsis
 * @param[in] y Coordinates of Y axsis
 * @param[in] cb Result callback function
 * @param[in] data Callback data
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_BUSY Previous operation is not finished yet
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully sent
 * @see widget_feed_mouse_event()
 * @see widget_feed_key_event()
 */
extern int widget_viewer_feed_access_event(widget_h handle, widget_access_event_type_e type, widget_access_event_info_s info, widget_ret_cb cb, void *data);

/**
 * @internal
 * @brief Sends a key event (for buffer type) to the provider (widget).
 * @remarks
 *    This is an ASYNCHRONOUS API.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @param[in] type Key event type
 * @param[in] keycode Code of key
 * @param[in] cb Result callback
 * @param[in] data Callback data
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_BUSY Previous operation is not finished yet
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully sent
 * @see widget_feed_mouse_event()
 * @see widget_feed_access_event()
 */
extern int widget_viewer_feed_key_event(widget_h handle, widget_key_event_type_e type, widget_key_event_info_s info, widget_ret_cb cb, void *data);

/**
 * @internal
 * @brief Sets pin-up status of the given handle.
 * @details If the widget supports the pinup feature,
 *   you can freeze the update of the given widget.
 *   But it is different from pause.
 *   The box will be updated and it will decide wheter update its content or not when the pinup is on.
 * @remarks
 *    This is an ASYNCHRONOUS API.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @param[in] flag Pinup value
 * @param[in] cb Result callback
 * @param[in] data Callback data
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid parameters
 * @see widget_ret_cb
 * @see widget_set_visibility()
 * @see widget_is_pinned_up()
 */
extern int widget_viewer_set_pinup(widget_h handle, int flag, widget_ret_cb cb, void *data);

/**
 * @internal
 * @brief Checks the PIN-UP status of the given handle.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid parameters
 * @retval 1 Box is pinned up
 * @retval 0 Box is not pinned up
 * @see widget_set_pinup()
 */
extern int widget_viewer_is_pinned_up(widget_h handle);

/**
 * @internal
 * @brief Checks the availability of the PIN-UP feature for the given handle.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval 1 If the box support Pinup feature
 * @retval 0 If the box does not support the Pinup feature
 * @see widget_is_pinned_up()
 * @see widget_set_pinup()
 */
extern int widget_viewer_has_pinup(widget_h handle);

/**
 * @internal
 * @brief Checks the existence of Glance Bar for the given handle.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval 1 If the box support the Glance Bar
 * @retval 0 If the box has no Glance Bar
 */
extern int widget_viewer_has_glance_bar(widget_h handle);

/**
 * @internal
 * @brief Creates Glance Bar of the given handle with the relative position from widget.
 * @remarks
 *    This is an ASYNCHRONOUS API.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @param[in] x 0.0 ~ 1.0
 * @param[in] y 0.0 ~ 1.0
 * @param[in] cb Result callback
 * @param[in] data Callback data
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully done
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_BUSY Previous operation is not finished yet
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @see widget_create_glance_bar()
 * @see widget_destroy_glance_bar()
 * @see widget_move_glance_bar()
 */
extern int widget_viewer_create_glance_bar(widget_h handle, double x, double y, widget_ret_cb cb, void *data);

/**
 * @internal
 * @brief Updates a position of the given Glance Bar.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @param[in] x 0.0 ~ 1.0, 0.0 indicates the coordinate X of left of widget
 * @param[in] y 0.0 ~ 1.0, 0.0 indicates the coordinate Y of top of widget
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_NONE If sending a request for updating position of the Glance Bar has been done successfully
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 */
extern int widget_viewer_move_glance_bar(widget_h handle, double x, double y);

/**
 * @internal
 * @brief Destroys the Glance Bar of the given handle if it is created.
 * @remarks
 *    This is an ASYNCHRONOUS API.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @param[in] cb Callback function
 * @param[in] data Callback data
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully done
 * @see widget_ret_cb
 */
extern int widget_viewer_destroy_glance_bar(widget_h handle, widget_ret_cb cb, void *data);

/**
 * @internal
 * @brief Checks the create status of the given widget handle.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval 0 Glance Bar is not created
 * @retval 1 Glance Bar is created
 */
extern int widget_viewer_glance_bar_is_created(widget_h handle);

/**
 * @internal
 * @brief Sets a function table for parsing the text content of a widget.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @param[in] gbar 1 for Glance Bar or 0
 * @param[in] ops
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully done
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @see widget_set_gbar_text_handler()
 */
extern int widget_viewer_set_text_handler(widget_h handle, int gbar, widget_script_operator_s ops);

/**
 * @internal
 * @brief Emits a text signal to the given widget only if it is a text type.
 * @since_tizen 2.3.1
 * @remarks
 *    This is an ASYNCHRONOUS API.
 *    This function is Asynchronous, so you will get result of add requst from @a cb, if you failed to send request to create a new widget,
 *    This function will returns proper error code
 * @param[in] handle Handle of a widget instance
 * @param[in] signal_name Emission string
 * @param[in] source Source string
 * @param[in] sx Start X
 * @param[in] sy Start Y
 * @param[in] ex End X
 * @param[in] ey End Y
 * @param[in] cb Result callback
 * @param[in] data Callback data
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid parameters
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully emitted
 * @see widget_ret_cb
 */
extern int widget_viewer_emit_text_signal(widget_h handle, widget_text_signal_s event_info, widget_ret_cb cb, void *data);

/**
 * @internal
 * @brief Sets a private data pointer to carry it using the given handle.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @param[in] data Data pointer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully registered
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @see widget_data()
 */
extern int widget_viewer_set_data(widget_h handle, void *data);

/**
 * @internal
 * @brief Gets a private data pointer which is carried by a given handle.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @retval data Data pointer
 * @retval @c NULL If there is no data
 * @see widget_set_data()
 */
extern void *widget_viewer_get_data(widget_h handle);

/**
 * @internal
 * @brief Subscribes an event for widgetes only in a given cluster and sub-cluster.
 * @details If you wrote a view-only client,
 *   you can receive the event of specific widgetes which belong to a given cluster/category.
 *   But you cannot modify their attributes (such as size, ...).
 * @since_tizen 2.3.1
 * @param[in] cluster Cluster ("*" can be used for subscribe all cluster's widgetes event; If you use the "*", value in the category will be ignored)
 * @param[in] category Category ("*" can be used for subscribe widgetes events of all category(sub-cluster) in a given "cluster")
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully requested
 * @see widget_unsubscribe_group()
 */
extern int widget_viewer_subscribe_group(const char *cluster, const char *sub_cluster);

/**
 * @internal
 * @brief Unsubscribes an event for the widgetes, but you will receive already added widgetes events.
 * @since_tizen 2.3.1
 * @param[in] cluster Cluster("*" can be used for subscribe all cluster's widgetes event; If you use the "*", value in the category will be ignored)
 * @param[in] category Category ("*" can be used for subscribe all sub-cluster's widgetes event in a given "cluster")
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully requested
 * @see widget_subscribe_group()
 */
extern int widget_viewer_unsubscribe_group(const char *cluster, const char *sub_cluster);

/**
 * @internal
 * @brief Subscribe events of widgetes which is categorized by given "category" string.
 *        "category" is written in the XML file of each widget manifest file.
 *        After subscribe the category, the master will send created event for all created widgetes,
 *        Also it will notify client when a new widget is created.
 * @since_tizen 2.3.1
 * @param[in] category Category name
 * @privlevel platform
 * @privilege %http://developer.samsung.com/tizen/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully requested
 * @see widget_unsubscribe_category()
 */
extern int widget_viewer_subscribe_category(const char *category);

/**
 * @internal
 * @brief Unsubscribe events of widgetes.
 * @since_tizen 2.3.1
 * @param[in] category Category name
 * @privlevel platform
 * @privilege %http://developer.samsung.com/tizen/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully requested
 * @see widget_subscribe_category()
 */
extern int widget_viewer_unsubscribe_category(const char *category);

/**
 * @internal
 * @brief Refreshes the group (cluster/sub-cluser (aka. category)).
 * @details This function will trigger the update of all widgetes in a given cluster/category group.
 * @since_tizen 2.3.1
 * @remarks Basically, a default widget system doesn't use the cluster/category concept.
 *    But you can use it. So if you decide to use it, then you can trigger the update of all widgetes in the given group.
 * @param[in] cluster Cluster ID
 * @param[in] category Sub-cluster ID
 * @param[in] force 1 if the boxes should be updated even if they are paused
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully requested
 * @see widget_refresh()
 */
extern int widget_viewer_refresh_group(const char *cluster, const char *category, int force);

/**
 * @brief Refreshes a widget.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] handle Handle of a widget instance
 * @param[in] force 1 if the box should be updated even if it is paused
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully requested
 * @see widget_refresh_group()
 */
extern int widget_viewer_refresh(widget_h handle, int force);

/**
 * @brief Gets Resource id of a widget content.
 * @details This function doesn't guarantee the life-cycle of the resource id.
 *   If the service provider destroyed the resource id, you will not know about it.
 *   So you should validate it before accessing it.
 * @since_tizen 2.3.1
 * * @param[in] handle Handle of a widget instance
 * @param[in] gbar 1 for Glance Bar or 0
 * @param[out] resouce_id result resource id of the widget
 * @return #WIDGET_STATUS_ERROR_NONE on success,
 *          otherwise an error code (see #WIDGET_STATUS_ERROR_XXX) on failure
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @see widget_get_resource_id()
 */
extern int widget_viewer_get_resource_id(const widget_h handle, int gbar, unsigned int *resouce_id);

/**
 * @internal
 * @brief Gets the Resource Id of a widget.
 * @details Even if a render process releases the Resource Id, the Resource Id will be kept before being released by widget_release_resource_id.
 *   You should release the resource id manually.
 * @remarks
 *    This is an ASYNCHRONOUS API.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @param[in] gbar 1 for Glance Bar or 0
 * @param[in] cb Callback function which will be called with result of acquiring widget resource id
 * @param[in] data Callback data
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully requested
 * @pre widget service system should support the ResourceId type buffer.
 *   The widget should be designed to use the buffer (or script).
 * @see widget_release_resource_id()
 * @see widget_ret_cb
 */
extern int widget_viewer_acquire_resource_id(widget_h handle, int gbar, widget_ret_cb cb, void *data);

/**
 * @internal
 * @brief Get the Resource Id of a widget for Extra buffer
 * @details Even if a render process(provider) released the Resource Id, it will be kept while release it by viewer.\n
 *          This will prevent from unexpected resource releasing for viewer.\n
 *          You should release this using widget_release_resource_id()
 * @remarks
 *    This is an ASYNCHRONOUS API.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @param[in] gbar 1 for Glance Bar or 0
 * @param[in] idx Index of extra buffer, it is limited to widget configuration
 * @param[in] cb Callback function which will be called with result of acquiring widget resource id
 * @param[in] data Callback data
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully requested
 * @pre widget service system should support the resource id type buffer.
 *      The widget should be designed to use the buffer (or script)
 * @see widget_release_resource_id()
 * @see widget_ret_cb
 */
extern int widget_viewer_acquire_extra_resource_id(widget_h handle, int gbar, int idx, widget_ret_cb cb, void *data);

/**
 * @brief Releases the Resource Id of a widget.
 * @details After a client gets a new Resource Id or does not need to keep the current Resource Id anymore, use this function to release it.
 *   If there is no user for a given Resource Id, the Resource Id will be destroyed.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] handle Handle of a widget instance
 * @param[in] gbar 1 for Glance Bar or 0
 * @param[in] resource_id Resource Id of given widget handle
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully done
 * @pre The Resource Id should be acquired by widget_acquire_resource_id
 * @see widget_acquire_resource_id()
 */
extern int widget_viewer_release_resource_id(widget_h handle, int gbar, unsigned int resource_id);

/**
 * @brief Updates a visible state of the widget.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @param[in] handle Handle of a widget instance
 * @param[in] state Configure the current visible state of a widget
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return #WIDGET_STATUS_ERROR_NONE on success,
 *          otherwise an error code (see #WIDGET_STATUS_ERROR_XXX) on failure
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_PERMISSION_DENIED Permission denied
 * @retval #WIDGET_STATUS_ERROR_ALREADY Input state is same to existing state
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 */
extern int widget_viewer_set_visibility(widget_h handle, widget_visible_state_e state);

/**
 * @internal
 * @brief Gets the current visible state of a widget.
 * @remarks
 *   If this returns WIDGET_VISIBLE_ERROR, you can get the reason of failure using get_last_result()
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @return widget_visible_state
 * @retval #WIDGET_SHOW widget is shown (Default state)
 * @retval #WIDGET_HIDE widget is hidden, Update timer is not frozen (but a user cannot receive any updated events; a user should refresh(reload) the content of a widget when a user make this show again)
 * @retval #WIDGET_HIDE_WITH_PAUSE widget is hidden, it will pause the update timer, but if a widget updates its contents, update event will occur
 * @retval #WIDGET_VISIBLE_ERROR To enlarge the size of this enumeration type
 */
extern widget_visible_state_e widget_viewer_get_visibility(widget_h handle);

/**
 * @internal
 * @brief Sets an update mode of the current widget.
 * @details If you set 1 for active update mode, you should get a buffer without updated event from provider.
 *   But if it is passive mode, you have to update content of a box when you get updated events.
 *   Default is Passive mode.
 * @since_tizen 2.3.1
 * @remarks
 *    This is an ASYNCHRONOUS API.
 *    This function is Asynchronous, so you will get result of add requst from @a cb, if you failed to send request to create a new widget,
 *    This function will returns proper error code
 * @param[in] handle Handle of a widget instance
 * @param[in] active_update 1 means active update, 0 means passive update (default)
 * @param[in] cb Result callback function
 * @param[in] data Callback data
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_BUSY
 * @retval #WIDGET_STATUS_ERROR_PERMISSION_DENIED
 * @retval #WIDGET_STATUS_ERROR_ALREADY
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully done
 * @see widget_ret_cb
 */
extern int widget_viewer_set_update_mode(widget_h handle, int active_update, widget_ret_cb cb, void *data);

/**
 * @internal
 * @brief Checks the active update mode of the given widget.
 * @remarks
 *   If this returns negative value, you can get the reason of failure using get_last_result()
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @return 0 on success, otherwise a negative error value
 * @retval 0 If passive mode
 * @retval 1 If active mode or error code
 */
extern int widget_viewer_is_active_update(widget_h handle);

/**
 * @internal
 * @brief Syncs manually
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @param[in] gbar 1 for Glance Bar or 0
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_NONE If success
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid handle
 * @see widget_set_manual_sync()
 * @see widget_manual_sync()
 */
extern int widget_viewer_sync_buffer(widget_h handle, int gbar);

/**
 * @internal
 * @brief Getting the damaged region info
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @param[in] gbar 1 for Glance Bar or 0
 * @param[out] region Readonly information for damaged area
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_NONE if success
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid handle
 */
extern int widget_viewer_get_damaged_region(widget_h handle, int gbar, const widget_damage_region_s *region);

/**
 * @internal
 * @brief Gets an alternative icon of the given widget instance.
 * @details If the box should be represented as a shortcut icon, this function will get the alternative icon.
 * @remarks
 *   If this returns @c NULL, you can get the reason of failure using get_last_result()
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @return const char *
 * @retval address Absolute path of an alternative icon file
 * @retval @c NULL widget has no alternative icon file
 * @see widget_alt_name()
 */
extern const char *widget_viewer_get_alternative_icon(widget_h handle);

/**
 * @internal
 * @brief Gets an alternative name of the given widget instance.
 * @details If the box should be represented as a shortcut name, this function will get the alternative name.
 * @remarks
 *   If this returns @c NULL, you can get the reason of failure using get_last_result()
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @return const char *
 * @retval name Alternative name of a widget
 * @retval @c NULL widget has no alternative name
 * @see widget_alt_icon()
 */
extern const char *widget_viewer_get_alternative_name(widget_h handle);

/**
 * @internal
 * @brief Gets a lock for a frame buffer.
 * @details This function should be used to prevent from rendering to the frame buffer while reading it.
 *   And the locking area should be short and must be released ASAP, or the render thread will be hanged.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @param[in] gbar 1 for Glance Bar or 0
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully done
 * @see widget_release_buffer_lock()
 */
extern int widget_viewer_acquire_buffer_lock(widget_h handle, int gbar);

/**
 * @internal
 * @brief Releases a lock of the frame buffer.
 * @details This function should be called ASAP after acquiring a lock of FB, or the render process will be blocked.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of a widget instance
 * @param[in] gbar 1 for Glance Bar or 0
 * @privlevel platform
 * @privilege %http://tizen.org/privilege/widget.viewer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_FAULT Unrecoverable error occurred
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully done
 * @see widget_acquire_buffer_lock()
 */
extern int widget_viewer_release_buffer_lock(widget_h handle, int gbar);

/**
 * @brief Sets options for controlling a widget sub-system.
 * @details
 *   #WIDGET_OPTION_FRAME_DROP_FOR_RESIZE
 *       While resizing the box, viewer doesn't want to know the updated frames of an old size content anymore.
 *       In that case, turn this on, the provider will not send the updated event to the viewer about an old content.
 *       So the viewer can reduce its burden to update unnecessary frames.
 *   #WIDGET_OPTION_MANUAL_SYNC
 *       If you don't want to update frames automatically, or you want only reload the frames by your hands, (manually)
 *       Turn this on.
 *       After turnning it on, you should sync it using widget_sync_buffer().
 *   #WIDGET_OPTION_SHARED_CONTENT
 *       If this option is turnned on, even though you create a new widget,
 *       if there are already added same instances that have same contents, the instance will not be created again.
 *       Instead of creating a new instance, a viewer will provide an old instance with a new handle.
 * @since_tizen 2.3.1
 * * @param[in] option Option which will be affected by this call
 * @param[in] state New value for given option
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Unknown option
 * @retval #WIDGET_STATUS_ERROR_FAULT Failed to change the state of option
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully changed
 * @see widget_get_option()
 * @see widget_sync_buffer()
 */
extern int widget_viewer_set_option(widget_option_type_e option, int state);

/**
 * @internal
 * @brief Gets options of a widget sub-system.
 * @remarks
 *   If this returns negative value, you can get the reason of failure using get_last_result()
 * @since_tizen 2.3.1
 * @param[in] option Type of option
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Invalid option
 * @retval #WIDGET_STATUS_ERROR_FAULT Failed to get option
 * @retval >=0 Value of given option (must be >=0)
 * @see widget_set_option()
 */
extern int widget_viewer_get_option(widget_option_type_e option);

/**
 * @internal
 * @brief Set a handle for launching an app by auto-launch feature
 * @details If a user clicks a box, which box enabled auto-launch option, the launcher_handler will be called.
 *          From that callback, you should launch an app using given ui-app id.
 * @since_tizen 2.3.1
 * @param[in] launch_handler Handle for launching an app manually
 * @param[in] data Callback data which will be given a data for launch_handler
 * @return #WIDGET_STATUS_ERROR_NONE on success,
 *          otherwise an error code (see #WIDGET_STATUS_ERROR_XXX) on failure
 * @retval #WIDGET_STATUS_ERROR_NONE Succeed to set new handle. there is no other cases
 */
extern int widget_viewer_set_auto_launch_handler(widget_auto_launch_handler_cb cb, void *data);

/**
 * @internal
 * @brief Get the last extra buffer index and its id.
 * @details
 *   If there is an event of #WIDGET_EVENT_WIDGET_EXTRA_BUFFER_CREATED or #WIDGET_EVENT_GBAR_EXTRA_BUFFER_CREATED,
 *                           #WIDGET_EVENT_WIDGET_EXTRA_BUFFER_DESTROYED or #WIDGET_EVENT_GBAR_EXTRA_BUFFER_DESTROYED
 *   you can use this to get the last created buffer info
 * @since_tizen 2.3.1
 * @param[in] handle widget handle
 * @param[in] gbar 1 if you want get the glance bar's info or 0
 * @param[out] idx Index of buffer
 * @param[out] resource_id Resource Id
 * @return status
 * @retval #WIDGET_STATUS_ERROR_NONE Successfully get
 * @retval #WIDGET_STATUS_ERROR_INVALID_PARAMETER Handle is not valid
 * @retval #WIDGET_STATUS_ERROR_NOT_EXIST There is no extra buffer
 */
extern int widget_viewer_get_affected_extra_buffer(widget_h handle, int gbar, int *idx, unsigned int *resource_id);

extern int widget_viewer_get_instance_id(widget_h handle, char **instance_id);

extern int widget_viewer_notify_orientation_of_viewer(int orientation);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
