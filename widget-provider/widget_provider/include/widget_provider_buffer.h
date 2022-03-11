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

/* Exported by widget_service library */
#include <widget_buffer.h>
#include <widget_service_internal.h>

#ifndef __WIDGET_PROVIDER_BUFFER_H
#define __WIDGET_PROVIDER_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file widget_provider_buffer.h
 * @brief This file declares API of libwidget_provider library
 * @since_tizen 2.3.1
 */

/**
 * @addtogroup CAPI_WIDGET_PROVIDER_MODULE
 * @{
 */

/**
 * @internal
 * @brief Send request for acquiring buffer of given type.
 * @since_tizen 2.3.1
 * @param[in] type #WIDGET_TYPE_WIDGET or #WIDGET_TYPE_GBAR, select the type of buffer. is it for WIDGET? or GBAR?
 * @param[in] pkgname Package name of a widget instance
 * @param[in] id Instance Id
 * @param[in] auto_align
 * @param[in] handler Event handler. Viewer will send the events such as mouse down,move,up, accessibilty event, etc,. via this callback function.
 * @param[in] data Callback data for event handler
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return widget_buffer
 * @retval widget buffer object handler
 * @retval @c NULL if it fails to acquire buffer
 * @see widget_provider_buffer_release()
 * @see widget_provider_buffer_acquire()
 */
extern widget_buffer_h widget_provider_buffer_create(widget_target_type_e type, const char *pkgname, const char *id, int auto_align, int (*handler)(widget_buffer_h, widget_buffer_event_data_t, void *), void *data);

/**
 * @internal
 * @brief Acquire a provider buffer
 * @since_tizen 2.3.1
 * @param[in] width Width of buffer
 * @param[in] height Height of buffer
 * @param[in] pixel_size Normally, use "4" for this. it is fixed.
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_INVALID_PARAMETER
 * @retval #WIDGET_ERROR_FAULT
 * @retval #WIDGET_ERROR_NONE
 * @see widget_provider_buffer_create()
 * @see widget_provider_buffer_release()
 */
extern int widget_provider_buffer_acquire(widget_buffer_h info, int width, int height, int pixel_size);

/**
 * @internal
 * @brief Acquire a extra buffer using given buffer handle
 * @since_tizen 2.3.1
 * @param[in] handle widget Buffer Handle
 * @param[in] idx Index of extra buffer, start from 0
 * @param[in] width Width of buffer
 * @param[in] height Height of buffer
 * @param[in] pixel_size Pixel size in bytes
 * @return int Result status of operation
 * @retval #WIDGET_ERROR_NONE Successfully acquired
 * @retval #WIDGET_ERROR_DISABLED Extra buffer acquire/release is not supported
 * @retval #WIDGET_ERROR_FAULT Unrecoverable error occurred
 * @retval #WIDGET_ERROR_OUT_OF_MEMORY Out of memory
 * @retval #WIDGET_ERROR_INVALID_PARAMTER Invalid parameter
 * @see widget_provider_buffer_extra_release()
 * @see widget_provider_buffer_extra_resource_id()
 */
extern int widget_provider_buffer_extra_acquire(widget_buffer_h handle, int idx, int width, int height, int pixel_size);

/**
 * @internal
 * @brief Resize a buffer to given size
 * @since_tizen 2.3.1
 * @param[in] info Buffer handle
 * @param[in] w Width in pixles size
 * @param[in] h Height in pixels size
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid parameters
 * @retval #WIDGET_ERROR_NONE Successfully resized
 */
extern int widget_provider_buffer_resize(widget_buffer_h info, int w, int h);

/**
 * @internal
 * @brief Get the buffer address of given livebowidgetr handle.
 * @since_tizen 2.3.1
 * @param[in] info Handle of widget buffer
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return address
 * @retval Address of buffer
 * @retval @c NULL
 * @see widget_provider_buffer_unref()
 */
extern void *widget_provider_buffer_ref(widget_buffer_h info);

/**
 * @internal
 * @brief Decrease the reference count of given buffer handle.
 * @since_tizen 2.3.1
 * @param[in] ptr Address that gets by provider_buffer_ref
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully un-referenced
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid parameters
 * @see widget_provider_buffer_ref()
 */
extern int widget_provider_buffer_unref(void *ptr);

/**
 * @internal
 * @brief Release the acquired buffer handle.
 * @since_tizen 2.3.1
 * @remarks This API will not destroy the buffer.
 * @param[in] info Handle of widget buffer
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid parameters
 * @retval #WIDGET_ERROR_NONE Successfully released
 * @see widget_provider_buffer_create()
 * @see widget_provider_buffer_acquire()
 */
extern int widget_provider_buffer_release(widget_buffer_h info);

/**
 * @internal
 * @brief Release the extra buffer handle
 * @since_tizen 2.3.1
 * @param[in] info Buffer handle
 * @param[in] idx Index of a extra buffer
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_INVALID_PARAMTER Invalid parameters
 * @retval #WIDGET_ERROR_NONE Successfully released
 * @see widget_provider_buffer_create()
 * @see widget_provider_buffer_extra_acquire()
 */
extern int widget_provider_buffer_extra_release(widget_buffer_h info, int idx);

/**
 * @internal
 * @brief Destroy Buffer Object
 * @since_tizen 2.3.1
 * @param[in] info Buffer handler
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully destroyed
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid parameter
 * @see widget_provider_buffer_create()
 * @see widget_provider_buffer_release()
 */
extern int widget_provider_buffer_destroy(widget_buffer_h info);

/**
 * @internal
 * @brief Make content sync with master.
 * @since_tizen 2.3.1
 * @param[in] info Handle of widget buffer
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid paramter
 * @retval #WIDGET_ERROR_NONE Successfully sync'd
 */
extern int widget_provider_buffer_sync(widget_buffer_h info);

/**
 * @internal
 * @brief Get the buffer type
 * @since_tizen 2.3.1
 * @param[in] info Handle of a widget buffer
 * @return target type GBAR or WIDGET
 * @retval #WIDGET_TYPE_WIDGET widget
 * @retval #WIDGET_TYPE_GBAR Glance Bar
 * @retval #WIDGET_TYPE_ERROR Undefined error
 */
extern widget_target_type_e widget_provider_buffer_type(widget_buffer_h info);

/**
 * @internal
 * @brief Get the package name of given buffer handler
 * @since_tizen 2.3.1
 * @param[in] info Handle of a widget buffer
 * @return const char *
 * @retval @c NULL
 * @retval pkgname package name
 * @see widget_provider_buffer_create()
 * @see widget_provider_buffer_id()
 */
extern const char *widget_provider_buffer_pkgname(widget_buffer_h info);

/**
 * @internal
 * @brief Get the Instance Id of given buffer handler
 * @since_tizen 2.3.1
 * @param[in] info Handle of widget buffer
 * @return const char *
 * @retval instance Id in string
 * @retval @c NULL
 * @see widget_provider_buffer_create()
 * @see widget_provider_buffer_pkgname()
 */
extern const char *widget_provider_buffer_id(widget_buffer_h info);

/**
 * @internal
 * @brief Give the URI of buffer information.
 * @details Every buffer information has their own URI.\n
 *          URI is consists with scheme and resource name.\n
 *          It looks like "pixmap://123456:4"\n
 *          "file:///tmp/abcd.png"\n
 *          "shm://1234"\n
 *          So you can recognize what method is used for sharing content by parse this URI.
 * @since_tizen 2.3.1
 * @param[in] info Handle of widget buffer
 * @return const char *
 * @retval uri URI of given buffer handler
 * @retval @c NULL
 * @see widget_provider_buffer_create()
 * @see widget_provider_buffer_acquire()
 */
extern const char *widget_provider_buffer_uri(widget_buffer_h info);

/**
 * @internal
 * @brief Get the size information of given buffer handler
 * @since_tizen 2.3.1
 * @param[in] info Handle of widget buffer
 * @param[out] w Width in pixels
 * @param[out] h Height in pixels
 * @param[out] pixel_size Bytes Per Pixels
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully get the information
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid parameters
 * @see widget_provider_buffer_acquire()
 * @see widget_provider_buffer_resize()
 */
extern int widget_provider_buffer_get_size(widget_buffer_h info, int *w, int *h, int *pixel_size);

/**
 * @internal
 * @brief Getting the PIXMAP id of mapped on this widget_buffer
 * @since_tizen 2.3.1
 * @param[in] info Handle of widget buffer
 * @return 0 on success, otherwise a negative error value
 * @retval \c 0 if fails
 * @retval resource id, has to be casted to unsigned int type
 * @pre widget_provider_buffer_acquire() must be called or this function will returns @c 0
 * @see widget_provider_buffer_uri()
 * @see widget_provider_buffer_create()
 * @see widget_provider_buffer_acquire()
 */
extern unsigned int widget_provider_buffer_resource_id(widget_buffer_h info);

/**
 * @internal
 */
extern unsigned int widget_provider_buffer_extra_resource_id(widget_buffer_h info, int idx);

/**
 * @internal
 * @brief Initialize the provider buffer system
 * @since_tizen 2.3.1
 * @param[in] disp Display information for handling the XPixmap type.
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_FAULT Failed to initialize the provider buffer system
 * @retval #WIDGET_ERROR_NONE Successfully initialized
 * @see widget_provider_buffer_fini()
 */
extern int widget_provider_buffer_init(void *disp);

/**
 * @internal
 * @brief Finalize the provider buffer system
 * @since_tizen 2.3.1
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully finalized
 * @see widget_provider_buffer_init()
 */
extern int widget_provider_buffer_fini(void);

/**
 * @internal
 * @brief Check whether current widget buffer support H/W(GEM) buffer
 * @since_tizen 2.3.1
 * @param[in] info Handle of widget buffer
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval 1 if support
 * @retval 0 if not supported
 * @pre widget_provider_buffer_create must be called first
 * @see widget_provider_buffer_create()
 */
extern int widget_provider_buffer_is_support_hw(widget_buffer_h info);

/**
 * @internal
 * @brief Create H/W(GEM) Buffer
 * @details Allocate system buffer for sharing the contents between provider & consumer without copying overhead.
 * @since_tizen 2.3.1
 * @param[in] info Handle of widget buffer
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully created
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid parameter or not supported
 * @retval #WIDGET_ERROR_FAULT Unrecoverable error occurred
 * @pre you can use this only if the provider_buffer_is_support_hw() function returns @c true(1)
 * @post N/A
 * @see widget_provider_buffer_is_support_hw()
 * @see widget_provider_buffer_destroy_hw()
 */
extern int widget_provider_buffer_create_hw(widget_buffer_h info);

/**
 * @internal
 * @brief Destroy H/W(GEM) Buffer
 * @since_tizen 2.3.1
 * @param[in] info Handle of widget buffer
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully destroyed
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid parameter
 * @see widget_provider_buffer_create_hw()
 */
extern int widget_provider_buffer_destroy_hw(widget_buffer_h info);

/**
 * @internal
 * @brief Get the H/W system mapped buffer address(GEM buffer) if a buffer support it.
 * @since_tizen 2.3.1
 * @param[in] info Handle of widget buffer
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @retval H/W system mapped buffer address
 * @retval @c NULL fails to get buffer address
 * @see widget_provider_buffer_create_hw()
 * @see widget_provider_buffer_destroy_hw()
 */
extern void *widget_provider_buffer_hw_addr(widget_buffer_h info);

/**
 * @internal
 * @brief Prepare the render buffer to write or read something on it.
 * @since_tizen 2.3.1
 * @param[in] info Handle of widget buffer
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE
 * @retval #WIDGET_ERROR_INVALID_PARAMETER
 * @retval #WIDGET_ERROR_FAULT
 * @see widget_provider_buffer_post_render()
 */
extern int widget_provider_buffer_pre_render(widget_buffer_h info);

/**
 * @internal
 * @brief Finish the render buffer acessing.
 * @since_tizen 2.3.1
 * @param[in] info Handle of widget buffer
 * @privlevel public
 * @privilege %http://tizen.org/privilege/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE
 * @retval #WIDGET_ERROR_INVALID_PARAMETER
 * @see widget_provider_buffer_pre_render()
 */
extern int widget_provider_buffer_post_render(widget_buffer_h info);

/**
 * @internal
 * @brief Get the user data registered on buffer handler.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of widget buffer
 * @retval User data
 * @pre widget_provider_buffer_set_user_data() function must be called before
 * @see widget_provider_buffer_set_user_data()
 */
extern void *widget_provider_buffer_user_data(widget_buffer_h handle);

/**
 * @internal
 * @brief Set a user data on given buffer handler.
 * @since_tizen 2.3.1
 * @param[in] handle Handle of widget buffer
 * @param[in] data User data
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully updated
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid parameter
 * @see widget_provider_buffer_user_data()
 */
extern int widget_provider_buffer_set_user_data(widget_buffer_h handle, void *data);

/**
 * @internal
 * @brief Find a buffer handler using given information
 * @since_tizen 2.3.1
 * @param[in] type #WIDGET_TYPE_WIDGET or #WIDGET_TYPE_GBAR can be used
 * @param[in] pkgname Package name
 * @param[in] id Instance Id
 * @return widget_buffer
 * @retval @c NULL If it fails to find one
 * @retval handle If it found
 * @see widget_provider_buffer_create()
 */
extern widget_buffer_h widget_provider_buffer_find_buffer(widget_target_type_e type, const char *pkgname, const char *id);

/**
 * @internal
 * @brief Get the stride information of given buffer handler
 * @since_tizen 2.3.1
 * @param[in] info Buffer handler
 * @return int stride size in bytes
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval stride_size in bytes
 */
extern int widget_provider_buffer_stride(widget_buffer_h info);

/**
 * @brief Send the updated event to the viewer using buffer info
 * @param[in] handle Buffer handler
 * @param[in] idx Index of buffer, #WIDGET_PRIMARY_BUFFER can be used to select the primary buffer if the widget has multiple buffer
 * @param[in] region Damaged region which should be updated
 * @param[in] for_gbar 1 if it indicates Glance bar or 0 for widget
 * @param[in] descfile Content description file if the widget is script type one.
 * @privlevel platform
 * @privilege %http://developer.samsung.com/privilege/core/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully sent update event
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid arguments
 * @retval #WIDGET_ERROR_FAULT There is error and it could not be recoverable
 * @see widget_provider_buffer_create()
 */
extern int widget_provider_send_buffer_updated(widget_buffer_h handle, int idx, widget_damage_region_s *region, int for_gbar, const char *descfile);

/**
 * @brief Send the updated event to the viewer using buffer info via given communication handler
 * @param[in] fd Communication handler, Socket file descriptor
 * @param[in] handle Buffer handler
 * @param[in] idx Index of buffer, #WIDGET_PRIMARY_BUFFER can be used to select the primary buffer if the widget has multiple buffer
 * @param[in] region Damaged region which should be updated
 * @param[in] for_gbar 1 if it indicates Glance bar or 0 for widget
 * @param[in] descfile Content description file if the widget is script type one.
 * @privlevel platform
 * @privilege %http://developer.samsung.com/privilege/core/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfully sent update event
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid arguments
 * @retval #WIDGET_ERROR_FAULT There is error and it could not be recoverable
 * @see widget_provider_buffer_create()
 */
extern int widget_provider_send_direct_buffer_updated(int fd, widget_buffer_h handle, int idx, widget_damage_region_s *region, int for_gbar, const char *descfile);

/**
 * @brief Getting the count of remained skippable frame.
 * @details If a buffer created, the viewer should not want to display unnecessary frames on the viewer,
 *          to use this, provider can gets the remained skippable frames.
 *          if the count is not reach to the ZERO, then the provider don't need to send update event to the viewer.
 *          But it highly depends on its implementations. this can be ignored by provider developer.
 * @param[in] info buffer handle
 * @privlevel platform
 * @privilege %http://developer.samsung.com/privilege/core/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid argument
 * @retval decimal count of skippable frames
 */
extern int widget_provider_buffer_frame_skip(widget_buffer_h info);

/**
 * @brief Clear the frame-skip value.
 * @details Sometimes viewer wants to clear this value if it detects that the updates is completed.
 * @param[in] info Buffer handle
 * @privlevel platform
 * @privilege %http://developer.samsung.com/privilege/core/widget.provider
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_ERROR_NONE Successfully cleared
 */
extern int widget_provider_buffer_clear_frame_skip(widget_buffer_h info);

/**
 * @brief Dump the buffer to a file
 * @details Used for debugging renderer.
 * @param[in] info Buffer handle
 * @param[in] file Output file
 * @privlevel platform
 * @privilege %http://developer.samsung.com/privilege/core/widget.provider
 * @return address on success, otherwise @c NULL for error
 */
extern void *widget_provider_buffer_dump_frame(widget_buffer_h info, int idx);
/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
/* End of a file */
