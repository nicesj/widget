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

#include <widget_script.h>
#include <widget_service.h>
#include <widget_service_internal.h>

#ifndef __WIDGET_PROVIDER_H
#define __WIDGET_PROVIDER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file widget_provider.h
 * @brief This file declares API of libwidget_provider library
 * @since_tizen 2.3.1
 */

/**
 * @addtogroup CAPI_WIDGET_PROVIDER_MODULE
 * @{
 */

/**
 * @internal
 * @brief Update the provider control
 * @since_tizen 2.3.1
 * @see provider_control()
 * THIS ENUM MUST HAS TO BE SYNC'd WITH THE DATA-PROVIDER-MASTER
 */
typedef enum widget_provider_ctrl {
    WIDGET_PROVIDER_CTRL_DEFAULT = 0x00,              /**< Set default control operation */
    WIDGET_PROVIDER_CTRL_MANUAL_TERMINATION = 0x01,   /**< Terminate process manually */
    WIDGET_PROVIDER_CTRL_MANUAL_REACTIVATION = 0x02,  /**< Reactivate process manually */
} widget_provider_ctrl_e;

/**
 * @internal
 * @brief Argument data structure for Dyanmic Box Instance events
 * @since_tizen 2.3.1
 */
struct widget_event_arg {
    enum _event_type {
        WIDGET_EVENT_NEW,                 /**< Master will send this to create a new widget instance */
        WIDGET_EVENT_RENEW,               /**< If the master detects any problems of your slave, it will terminate slave provider.
                                               and after reactivating the provider slave, this request will be delievered to create a widget instance again */

        WIDGET_EVENT_DELETE,              /**< Master will send this to delete a widget instance */

        WIDGET_EVENT_CONTENT_EVENT,       /**< Any events are generated from your widget or Glance Bar, this event will be sent to you */
        WIDGET_EVENT_CLICKED,             /**< If a widget is clicked, the master will send this event */
        WIDGET_EVENT_TEXT_SIGNAL,         /**< Text type widget or Glance Bar will generate this event */

        WIDGET_EVENT_RESIZE,              /**< If a widget is resized, the master will send this event */
        WIDGET_EVENT_SET_PERIOD,          /**< To change the update period of a widget */
        WIDGET_EVENT_CHANGE_GROUP,        /**< To change the group(cluster/sub-cluster) of a widget */
        WIDGET_EVENT_PINUP,               /**< To make pin up of a widget */

        WIDGET_EVENT_UPDATE_CONTENT,      /**< It's time to update the content of a widget */

        WIDGET_EVENT_PAUSE,               /**< Freeze all timer and go to sleep mode */
        WIDGET_EVENT_RESUME,              /**< Thaw all timer and wake up */

        WIDGET_EVENT_GBAR_CREATE,         /**< Only for the buffer type */
        WIDGET_EVENT_GBAR_DESTROY,        /**< Only for the buffer type */
        WIDGET_EVENT_GBAR_MOVE,           /**< Only for the buffer type */

        WIDGET_EVENT_WIDGET_PAUSE,        /**< Freeze the update timer of a specified widget */
        WIDGET_EVENT_WIDGET_RESUME,       /**< Thaw the update timer of a specified widget */

		WIDGET_EVENT_VIEWER_CONNECTED,    /**< A new viewer is connected */
		WIDGET_EVENT_VIEWER_DISCONNECTED, /**< The viewer is disconnected */

		WIDGET_EVENT_ORIENTATION,         /**< Orientation is changed */
		WIDGET_EVENT_CTRL_MODE            /**< Control provider */
    } type;
    const char *pkgname; /**< Package name of a widget */
    const char *id; /**< Instance Id of a widget */

    union _event_data {
        struct _gbar_create {
            int w;                   /**< Glance Bar buffer is created with width "w" */
            int h;                   /**< Glance Bar buffer is created with height "h" */

            double x;                /**< Relative X position of a widget from this Glance Bar */
            double y;                /**< Relative Y position of a widget from this Glance Bar */
        } gbar_create;

        struct _gbar_destroy {
            int reason;              /**< Error (status) code */
        } gbar_destroy;

        struct _gbar_move {
            int w;                   /**< Glance Bar buffer width */
            int h;                   /**< Glance Bar buffer height */

            double x;                /**< Relative X position of a widget from this Glance Bar */
            double y;                /**< Relative Y position of a widget from this Glance Bar */
        } gbar_move;

        struct _widget_create {
            const char *content;     /**< Content info */
            int timeout;             /**< Timeout */
            int has_script;          /**< widget has script (buffer would be created from the master) */
            double period;           /**< Update period */
            const char *cluster;     /**< Cluster ID of this widget instance */
            const char *category;    /**< Sub-cluster ID of this widget instance */
            int skip_need_to_create; /**< Is this widget need to check the "need_to_create"? */
            int width;               /**< Width of a widget content in pixels */
            int height;              /**< Height of a widget content in pixels */
            const char *abi;         /**< ABI tag of this widget */
            char *out_content;       /**< Output content */
            char *out_title;         /**< Output title */
            int out_is_pinned_up;    /**< Is this pinned up? */
            const char *direct_addr; /** Event path for sending updated event to the viewer directly */
			int degree;              /** Orientation */
        } widget_create; /**< Event information for the newly created instance */

        struct _widget_recreate {
            const char *content;     /**< Content info */
            int timeout;             /**< Timeout */
            int has_script;          /**< widget has script (buffer would be created from the master) */
            double period;           /**< Update period */
            const char *cluster;     /**< Cluster ID of this widget instance */
            const char *category;    /**< Sub-cluster ID of this widget instance */
            int width;               /**< Width of a widget content in pixels */
            int height;              /**< Height of a widget content in pixels */
            const char *abi;         /**< ABI tag of this widget */
            char *out_content;       /**< Output content */
            char *out_title;         /**< Output title */
            int out_is_pinned_up;    /**< Is this pinned up? */
            int hold_scroll;         /**< The scroller which is in viewer is locked */
            int active_update;       /**< Need to know, current update mode */
            const char *direct_addr; /**< Event path for sending updated event to the viewer directly */
			int degree;              /**< Orientation */
        } widget_recreate; /**< Event information for the re-created instance */

        struct _widget_destroy { /**< This enumeration value must has to be sync with data-provider-master */
            widget_destroy_type_e type;
        } widget_destroy; /**< Destroyed */

        struct _content_event {
            const char *signal_name;              /**< Event string */
            const char *source;                /**< Object ID which makes event */
            struct widget_event_info info; /**< Extra information */
        } content_event; /**< script */

        struct _clicked {
            const char *event; /**< Event type, currently only "click" supported */
            double timestamp;  /**< Timestamp of event occurred */
            double x;          /**< X position of the click event */
            double y;          /**< Y position of the click event */
        } clicked; /**< clicked */

        struct _text_signal {
            const char *signal_name;              /**< Event string */
            const char *source;                /**< Object ID which makes event */
            struct widget_event_info info; /**< Extra information */
        } text_signal; /**< text_signal */

        struct _resize {
            int w; /**< New width of a widget */
            int h; /**< New height of a widget */
        } resize; /**< resize */

        struct _set_period {
            double period; /**< New period */
        } set_period; /**< set_period */

        struct _change_group {
            const char *cluster;  /**< Cluster information is changed */
            const char *category; /**< Sub-cluster information is changed */
        } change_group; /**< change_group */

        struct _pinup {
            int state;          /**< Current state of Pin-up */
            char *content_info; /**< out value */
        } pinup; /**< pinup */

        struct _update_content {
            const char *cluster;  /**< Cluster information */
            const char *category; /**< Sub-cluster information */
            const char *content;  /**< Content information */
            int force;            /**< Updated by fault */
        } update_content; /**< update_content */

        struct _pause {
            double timestamp; /**< Timestamp of the provider pause event occurred */
        } pause; /**< pause */

        struct _resume {
            double timestamp; /**< Timestamp of the provider resume event occurred */
        } resume; /**< resume */

        struct _widget_pause {
            /**< widget is paused */
        } widget_pause;

        struct _widget_resume {
            /**< widget is resumed */
        } widget_resume;

        struct _update_mode {
            int active_update; /**< 1 for Active update mode or 0 */
        } update_mode; /**< Update mode */

		struct _viewer_connected {
			const char *direct_addr;
		} viewer_connected;
		struct _viewer_disconnected {
			const char *direct_addr;
		} viewer_disconnected;

		struct _orientation {
			int degree;       /**< Changed orientation */
		} orientation;

		struct _ctrl_mode {
			int cmd;
			int value;
		} ctrl_mode;
    } info;
};

/**
 * @internal
 * @brief Event Handler tables for managing the life-cycle of widget instances
 * @since_tizen 2.3.1
 */
typedef struct widget_event_table {
    int (*widget_create)(struct widget_event_arg *arg, int *width, int *height, double *priority, void *data); /**< new */
    int (*widget_destroy)(struct widget_event_arg *arg, void *data); /**< delete */

    /**
     * @note
     * Recover from the fault of slave
     * If a widget is deleted by accidently, it will be re-created by provider master,
     * in that case you will get this event callback.
     * From here the widget developer should recover its context.
     */
    int (*widget_recreate)(struct widget_event_arg *arg, void *data); /**< Re-create */

    int (*widget_pause)(struct widget_event_arg *arg, void *data); /**< Pause a specific widget */
    int (*widget_resume)(struct widget_event_arg *arg, void *data); /**< Resume a specific widget */

    int (*content_event)(struct widget_event_arg *arg, void *data); /**< Content event */
    int (*clicked)(struct widget_event_arg *arg, void *data); /**< Clicked */
    int (*text_signal)(struct widget_event_arg *arg, void *data); /**< Text Signal */
    int (*resize)(struct widget_event_arg *arg, void *data); /**< Resize */
    int (*set_period)(struct widget_event_arg *arg, void *data); /**< Set Period */
    int (*change_group)(struct widget_event_arg *arg, void *data); /**< Change group */
    int (*pinup)(struct widget_event_arg *arg, void *data); /**< Pin up */
    int (*update_content)(struct widget_event_arg *arg, void *data); /**< Update content */
	int (*orientation)(struct widget_event_arg *arg, void *data); /**< Orientation changed event */

    /**
     * @note
     * Visibility changed event
     * these "pause" and "resume" callbacks are different with "widget_pause" and "widget_resume",
     */
    int (*pause)(struct widget_event_arg *arg, void *data); /**< Pause the service provider */
    int (*resume)(struct widget_event_arg *arg, void *data); /**< Resume the service provider */
    /**
     * @note
     * service status changed event
     * If a provider lost its connection to master,
     * the provider should clean all its instances up properly.
     * and terminate itself if it is required.
     * or waiting a new connection request from the master.
     * The master will try to re-connect to the provider again if the provider doesn't support the multiple instance (process)
     * "connected" callback will be called after the provider-app gets "launch"(app-control) request.
     */
    int (*disconnected)(struct widget_event_arg *arg, void *data); /**< Disconnected from master */
    int (*connected)(struct widget_event_arg *arg, void *data); /**< Connected to master */

    /**
     * @note
     * Only for the buffer type
     * 'gbar' is short from "Glance Bar"
     */
    int (*gbar_create)(struct widget_event_arg *arg, void *data); /**< Glance Bar is created */
    int (*gbar_destroy)(struct widget_event_arg *arg, void *data); /**< Glance Bar is destroyed */
    int (*gbar_move)(struct widget_event_arg *arg, void *data); /**< Glance Bar is moved */

    int (*update_mode)(struct widget_event_arg *arg, void *data); /**< Update mode */

	int (*viewer_connected)(struct widget_event_arg *arg, void *data); /**< New viewer is connected */
	int (*viewer_disconnected)(struct widget_event_arg *arg, void *data); /**< The viewer is disconnected */

	int (*ctrl_mode)(struct widget_event_arg *arg, void *data); /* Change the mode of provider */
} *widget_event_table_h;

/**
 * @internal
 * Damaged region for updated event
 */
typedef struct widget_damage_region {
    int x; /**< Damaged region, Coordinates X of Left Top corner */
    int y; /**< Damaged region, Coordinates Y of Left Top corner */
    int w; /**< Width of Damaged region from X */
    int h; /**< Height of Damaged region from Y */
} widget_damage_region_s;

/**
 * @internal
 * @brief Initialize the provider service
 * @since_tizen 2.3.1
 * @details Some provider doesn't want to access environment value.\n
 *          This API will get some configuration info via argument instead of environment value.
 * @param[in] disp Display object, if you don't know what this is, set @c NULL
 * @param[in] name Slave name which is given by the master provider.
 * @param[in] table Event handler table
 * @param[in] data callback data
 * @param[in] prevent_overwrite 0 if newly created content should overwrite its old one, 1 to rename the old one before create new one.
 * @param[in] com_core_use_thread 1 to create a thread for socket communication, called packet pump, it will make a best performance for emptying the socket buffer.
 *                                if there is heavy communication, the system socket buffer can be overflow'd, to protect from socket buffer overflow, creating a
 *                                thread. created thread will do flushing all input stream of socket buffer, so the system will not suffer from buffer overflow.
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully initialized
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid arguments
 * @retval #WIDGET_ERROR_OUT_OF_MEMORY Out of memory
 * @retval #WIDGET_ERROR_ALREADY_STARTED Already initialized
 * @retval #WIDGET_ERROR_FAULT Failed to initialize
 * @see widget_provider_fini()
 */
extern int widget_provider_prepare_init(const char *abi, const char *accel, int secured);
extern int widget_provider_init(void *disp, const char *name, widget_event_table_h table, void *data, int prevent_overwrite, int com_core_use_thread);

/**
 * @internal
 * @brief Finalize the provider service
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @retval callback data which is registered via provider_init. it should be cared by developer, if it should be released.
 * @see widget_provider_init()
 */
extern void *widget_provider_fini(void);

/**
 * @internal
 * @brief Send the hello signal to the master
 * @since_tizen 2.3.1
 * @remarks\n
 *        Master will activate connection of this only if a provider send this hello event to it.\n
 *        or the master will reject all requests from this provider.
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully sent
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid arguments
 * @retval #WIDGET_ERROR_FAULT Failed to initialize
 */
extern int widget_provider_send_hello(void);

/**
 * @internal
 * @brief Send the hello signal to the master and receive the request of instance creation.
 * @since_tizen 2.3.1
 * @remarks\n
 *        Master will activate connection of this only if a provider send this hello event to it.\n
 *        or the master will reject all requests from this provider.
 * @param[in] pkgname Package name which is an updated widget.
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully sent
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid arguments
 * @retval #WIDGET_ERROR_FAULT Failed to initialize
 */
extern int widget_provider_send_hello_sync(const char *pkgname);

/**
 * @internal
 * @brief Send the ping message to the master to notify that your provider is working properly.
 *        if the master couldn't get this ping master in ping interval, your provider will be dealt as a faulted one.
 *        you have to call this periodically. if you want keep your service provider alive.
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully sent
 * @retval #WIDGET_ERROR_INVALID_PARAMETER provider is not initialized
 * @retval #WIDGET_ERROR_FAULT Failed to send ping message
 */
extern int widget_provider_send_ping(void);

/**
 * @internal
 * @brief Send the updated event to the master regarding updated widget content.
 * @details\n
 *  If you call this function, the output filename which is pointed by @a id will be renamed to uniq filename.\n
 *  So you cannot access the output file after this function calls.\n
 *  It only be happend when you set "prevent_overwrite" to "1" for widget_provider_init().\n
 *  But if the @a prevent_overwrite option is disabled, the output filename will not be changed.\n
 *  This is only happens when the widget uses file for content sharing method as image or description.
 * @since_tizen 2.3.1
 * @param[in] pkgname Package name which is an updated widget.
 * @param[in] id Instance ID of an updated widget
 * @param[in] w Width of an updated content
 * @param[in] h Height of an updated content
 * @param[in] for_gbar 1 for updating GBAR or 0 (widget)
 * @param[in] descfile if the widget(or gbar) is developed as script type, this can be used to deliever the name of a content description file.
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully sent update event
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid arguments
 * @retval #WIDGET_ERROR_FAULT There is error and it could not be recoverable
 * @see widget_provider_send_direct_updated()
 */
extern int widget_provider_send_updated(const char *pkgname, const char *id, int idx, widget_damage_region_s *region, int for_gbar, const char *descfile);

/**
 * @brief Send the updated event to the viewer directly.
 * @details\n
 *  If you call this function, the output filename which is pointed by @a id will be renamed to uniq filename.\n
 *  So you cannot access the output file after this function calls.\n
 *  But if the @a prevent_overwrite option is enabled, the output filename will not be changed.\n
 *  This is only happens when the widget uses file for content sharing method as image or description.
 * @param[in] fd connection handler
 * @param[in] pkgname Package name which is an updated widget
 * @param[in] id Instance ID of an updated widget
 * @param[in] w Width of an updated content
 * @param[in] h Height of an updated content
 * @param[in] for_gbar 1 for updating GBAR or 0 (widget)
 * @param[in] descfile if the widget(or gbar) is developed as script type, this can be used to deliever the name of a content description file.
 * @privlevel platform
 * @privilege %http://developer.samsung.com/privilege/core/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully sent update event
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid arguments
 * @retval #WIDGET_ERROR_FAULT There is error and it could not be recoverable
 * @see widget_provider_send_updated()
 */
extern int widget_provider_send_direct_updated(int fd, const char *pkgname, const char *id, int idx, widget_damage_region_s *region, int for_gbar, const char *descfile);

/**
 * @internal
 * @brief Send extra information to the master regarding given widget instance.
 * @since_tizen 2.3.1
 * @param[in] priority Priority of an updated content, could be in between 0.0 and 1.0
 * @param[in] content_info Content string, used by widget.
 * @param[in] title Title string that is summarizing the content to a short text
 * @param[in] icon Absolute path of an icon file to use it for representing the contents of a box alternatively
 * @param[in] name Name for representing the contents of a box alternatively
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully sent update event
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid arguments
 * @retval #WIDGET_ERROR_FAULT There is error and it could not be recoverable
 */
extern int widget_provider_send_extra_info(const char *pkgname, const char *id, double priority, const char *content_info, const char *title, const char *icon, const char *name);

/**
 * @internal
 * @brief If the client requests async update mode, service provider must has to send update begin when it really start to update its contents
 * @since_tizen 2.3.1
 * @param[in] pkgname Package name of widget
 * @param[in] id Instance Id of widget
 * @param[in] priority Priority of current content of widget
 * @param[in] content_info Content info could be come again via widget_create() interface
 * @param[in] title A sentence for describing content of widget
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE
 * @retval #WIDGET_ERROR_INVALID_PARAMETER
 * @retval #WIDGET_ERROR_FAULT
 * @see widget_provider_send_widget_update_end()
 */
extern int widget_provider_send_widget_update_begin(const char *pkgname, const char *id, double priority, const char *content_info, const char *title);

/**
 * @internal
 * @brief If the client requests async update mode, service provider must has to send update end when the update is done
 * @since_tizen 2.3.1
 * @param[in] pkgname Package name of widget
 * @param[in] id Instance Id of widget
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE
 * @retval #WIDGET_ERROR_INVALID_PARAMETER
 * @retval #WIDGET_ERROR_FAULT
 * @see widget_provider_send_widget_update_begin()
 */
extern int widget_provider_send_widget_update_end(const char *pkgname, const char *id);

/**
 * @internal
 * @brief If the client requests async update mode, service provider must has to send update begin when it really start to update its contents
 * @since_tizen 2.3.1
 * @param[in] pkgname Package name of widget
 * @param[in] id Instance Id of widget
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE
 * @retval #WIDGET_ERROR_INVALID_PARAMETER
 * @retval #WIDGET_ERROR_FAULT
 * @see widget_provider_send_gbar_update_end()
 */
extern int widget_provider_send_gbar_update_begin(const char *pkgname, const char *id);

/**
 * @internal
 * @brief If the client requests async update mode, service provider must has to send update end when it really finish the update its contents
 * @since_tizen 2.3.1
 * @param[in] pkgname Package name of widget
 * @param[in] id Instance Id of widget
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE
 * @retval #WIDGET_ERROR_INVALID_PARAMETER
 * @retval #WIDGET_ERROR_FAULT
 * @see widget_provider_send_gbar_update_begin()
 */
extern int widget_provider_send_gbar_update_end(const char *pkgname, const char *id);

/**
 * @internal
 * @brief Send the deleted event of specified widget instance
 * @since_tizen 2.3.1
 * @param[in] pkgname Package name of the widget
 * @param[in] id widget instance ID
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE
 * @retval #WIDGET_ERROR_INVALID_PARAMETER
 * @retval #WIDGET_ERROR_FAULT
 * @see widget_provider_send_faulted()
 * @see widget_provider_send_hold_scroll()
 */
extern int widget_provider_send_deleted(const char *pkgname, const char *id);

/**
 * @internal
 * @brief If you want to use the fault management service of the master provider,\n
 *        Before call any functions of a widget, call this.
 * @since_tizen 2.3.1
 * @param[in] pkgname Package name of the widget
 * @param[in] id Instance ID of the widget
 * @param[in] funcname Function name which will be called
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully sent
 * @retval #WIDGET_ERROR_INVALID_PARAMETER
 * @retval #WIDGET_ERROR_FAULT
 * @see widget_provider_send_call()
 */
extern int widget_provider_send_ret(const char *pkgname, const char *id, const char *funcname);

/**
 * @internal
 * @brief If you want to use the fault management service of the master provider,\n
 *        After call any functions of a widget, call this.
 * @since_tizen 2.3.1
 * @param[in] pkgname Package name of the widget
 * @param[in] id Instance ID of the widget
 * @param[in] funcname Function name which is called by the slave
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully sent
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid paramters
 * @retval #WIDGET_ERROR_FAULT Failed to send a request
 * @see widget_provider_send_ret()
 */
extern int widget_provider_send_call(const char *pkgname, const char *id, const char *funcname);

/**
 * @internal
 * @brief If you want to send the fault event to the master provider,
 *        Use this API
 * @since_tizen 2.3.1
 * @param[in] pkgname Package name of the widget
 * @param[in] id ID of the widget instance
 * @param[in] funcname Reason of the fault error
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully sent
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid parameters
 * @retval #WIDGET_ERROR_FAULT Failed to send a request
 * @see widget_provider_send_deleted()
 * @see widget_provider_send_hold_scroll()
 * @see widget_provider_send_access_status()
 * @see widget_provider_send_key_status()
 * @see widget_provider_send_request_close_gbar()
 */
extern int widget_provider_send_faulted(const char *pkgname, const char *id, const char *funcname);

/**
 * @internal
 * @brief If you want notify to viewer to seize the screen,
 *        prevent moving a box from user event
 * @since_tizen 2.3.1
 * @param[in] pkgname Package name of the widget
 * @param[in] id ID of the widget instance
 * @param[in] seize 1 if viewer needs to hold a box, or 0
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully sent
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid parameters
 * @retval #WIDGET_ERROR_FAULT Failed to send a request
 * @see widget_provider_send_faulted()
 * @see widget_provider_send_deleted()
 * @see widget_provider_send_access_status()
 * @see widget_provider_send_key_status()
 * @see widget_provider_send_request_close_gbar()
 */
extern int widget_provider_send_hold_scroll(const char *pkgname, const char *id, int seize);

/**
 * @internal
 * @brief Notify to viewer for accessiblity event processing status.
 * @since_tizen 2.3.1
 * @param[in] pkgname Package name of widget
 * @param[in] id Instance Id of widget
 * @param[in] status \n
 *    You should returns one in below list, those are defined in widget-service.h header\n
 *    #WIDGET_ACCESS_STATUS_ERROR If there is some error while processing accessibility events\n
 *    #WIDGET_ACCESS_STATUS_DONE Successfully processed\n
 *    #WIDGET_ACCESS_STATUS_FIRST Reach to the first focusable object\n
 *    #WIDGET_ACCESS_STATUS_LAST Reach to the last focusable object\n
 *    #WIDGET_ACCESS_STATUS_READ Read done
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully sent
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #WIDGET_ERROR_FAULT Failed to send a status
 * @see widget_provider_send_deleted()
 * @see widget_provider_send_faulted()
 * @see widget_provider_send_hold_scroll()
 * @see widget_provider_send_key_status()
 * @see widget_provider_send_request_close_gbar()
 */
extern int widget_provider_send_access_status(const char *pkgname, const char *id, int status);

/**
 * @internal
 * @brief Notify to viewer for key event processing status.
 * @since_tizen 2.3.1
 * @param[in] pkgname Package name of widget
 * @param[in] id Instance Id of widget
 * @param[in] status \n
 *          Send the key event processing result to the viewer\n
 *        #WIDGET_KEY_STATUS_ERROR If there is some error while processing key event\n
 *        #WIDGET_KEY_STATUS_DONE Key event is successfully processed\n
 *        #WIDGET_KEY_STATUS_FIRST Reach to the first focusable object\n
 *        #WIDGET_KEY_STATUS_LAST Reach to the last focusable object
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully sent
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid paramter
 * @retval #WIDGET_ERROR_FAULT Failed to send a status
 * @see widget_provider_send_deleted()
 * @see widget_provider_send_faulted()
 * @see widget_provider_send_hold_scroll()
 * @see widget_provider_send_access_status()
 * @see widget_provider_send_request_close_gbar()
 */
extern int widget_provider_send_key_status(const char *pkgname, const char *id, int status);

/**
 * @internal
 * @brief Send request to close the Glance Bar if it is opened.
 * @since_tizen 2.3.1
 * @param[in] pkgname Package name to send a request to close the Glance Bar
 * @param[in] id Instance Id of widget which should close its Glance Bar
 * @param[in] reason Reserved for future use
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully sent
 * @retval #WIDGET_ERROR_FAULT Failed to send request to the viewer
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid argument
 * @see widget_provider_send_deleted()
 * @see widget_provider_send_faulted()
 * @see widget_provider_send_hold_scroll()
 * @see widget_provider_send_access_status()
 * @see widget_provider_send_key_status()
 */
extern int widget_provider_send_request_close_gbar(const char *pkgname, const char *id, int reason);

/**
 * @internal
 * @brief Change the provider control policy.
 * @since_tizen 2.3.1
 * @param[in] ctrl enumeration list - enum PROVIDER_CTRL
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully changed
 */
extern int widget_provider_control(int ctrl);

extern void *widget_provider_callback_data(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif

/* End of a file */
