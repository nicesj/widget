#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <linux/input.h>

#include <glib.h>

#include <dlog.h>
#include <widget_errno.h>
#include <widget_service.h>
#include <widget_buffer.h>

#include "debug.h"
#include "event.h"
#include "provider_buffer_internal.h"
#include "dlist.h"

int errno;

struct event_object {
	enum event_state state;
	widget_buffer_h handler;

	int x;
	int y;
	double timestamp;
};

static struct info {
	int fd;
	guint id;
	int readsize;
	int timestamp_updated;
	struct dlist *event_object_list;

	struct event_data event_data;
	struct input_event input_event;
	struct widget_buffer_event_data last_event_data;
} s_info = {
	.fd = -1,
	.id = 0,
	.readsize = 0,
	.timestamp_updated = 0,
	.event_object_list = NULL,
};

static void update_timestamp(struct input_event *event)
{
	/*
	 * Input event uses timeval instead of timespec,
	 * but its value is same as MONOTIC CLOCK TIME
	 * So we should handles it properly.
	 */
	s_info.event_data.tv = (double)event->time.tv_sec + (double)event->time.tv_usec / 1000000.0f;
	s_info.timestamp_updated = 1;
}

static void processing_ev_abs(struct input_event *event)
{
	switch (event->code) {
#if defined(ABS_X)
	case ABS_X:
		break;
#endif
#if defined(ABS_Y)
	case ABS_Y:
		break;
#endif
#if defined(ABS_Z)
	case ABS_Z:
		break;
#endif
#if defined(ABS_RX)
	case ABS_RX:
		break;
#endif
#if defined(ABS_RY)
	case ABS_RY:
		break;
#endif
#if defined(ABS_RZ)
	case ABS_RZ:
		break;
#endif
#if defined(ABS_THROTTLE)
	case ABS_THROTTLE:
		break;
#endif
#if defined(ABS_RUDDER)
	case ABS_RUDDER:
		break;
#endif
#if defined(ABS_WHEEL)
	case ABS_WHEEL:
		break;
#endif
#if defined(ABS_GAS)
	case ABS_GAS:
		break;
#endif
#if defined(ABS_BRAKE)
	case ABS_BRAKE:
		break;
#endif
#if defined(ABS_HAT0X)
	case ABS_HAT0X:
		break;
#endif
#if defined(ABS_HAT0Y)
	case ABS_HAT0Y:
		break;
#endif
#if defined(ABS_HAT1X)
	case ABS_HAT1X:
		break;
#endif
#if defined(ABS_HAT1Y)
	case ABS_HAT1Y:
		break;
#endif
#if defined(ABS_HAT2X)
	case ABS_HAT2X:
		break;
#endif
#if defined(ABS_HAT2Y)
	case ABS_HAT2Y:
		break;
#endif
#if defined(ABS_HAT3X)
	case ABS_HAT3X:
		break;
#endif
#if defined(ABS_HAT3Y)
	case ABS_HAT3Y:
		break;
#endif
#if defined(ABS_PRESSURE)
	case ABS_PRESSURE:
		break;
#endif
#if defined(ABS_TILT_X)
	case ABS_TILT_X:
		break;
#endif
#if defined(ABS_TILT_Y)
	case ABS_TILT_Y:
		break;
#endif
#if defined(ABS_TOOL_WIDTH)
	case ABS_TOOL_WIDTH:
		break;
#endif
#if defined(ABS_VOLUME)
	case ABS_VOLUME:
		break;
#endif
#if defined(ABS_MISC)
	case ABS_MISC:
		break;
#endif
#if defined(ABS_DISTANCE)
	case ABS_DISTANCE:
		s_info.event_data.distance = event->value;
		break;
#endif
#if defined(ABS_MT_POSITION_X)
	case ABS_MT_POSITION_X:
		s_info.event_data.x = event->value;
		break;
#endif
#if defined(ABS_MT_POSITION_Y)
	case ABS_MT_POSITION_Y:
		s_info.event_data.y = event->value;
		break;
#endif
#if defined(ABS_MT_SLOT)
	case ABS_MT_SLOT:
		s_info.event_data.slot = event->value;
		break;
#endif
#if defined(ABS_MT_TRACKING_ID)
	case ABS_MT_TRACKING_ID:
		s_info.event_data.device = event->value;
		break;
#endif
#if defined(ABS_MT_TOUCH_MAJOR)
	case ABS_MT_TOUCH_MAJOR:
		s_info.event_data.touch.major = event->value;
		break;
#endif
#if defined(ABS_MT_TOUCH_MINOR)
	case ABS_MT_TOUCH_MINOR:
		s_info.event_data.touch.minor = event->value;
		break;
#endif
#if defined(ABS_MT_WIDTH_MAJOR)
	case ABS_MT_WIDTH_MAJOR:
		s_info.event_data.width.major = event->value;
		break;
#endif
#if defined(ABS_MT_WIDTH_MINOR)
	case ABS_MT_WIDTH_MINOR:
		s_info.event_data.width.minor = event->value;
		break;
#endif
#if defined(ABS_MT_ORIENTATION)
	case ABS_MT_ORIENTATION:
		s_info.event_data.orientation = event->value;
		break;
#endif
#if defined(ABS_MT_PRESSURE)
	case ABS_MT_PRESSURE:
		s_info.event_data.pressure = event->value;
		break;
#endif
#if defined(ABS_MT_TOOL_X)
	case ABS_MT_TOOL_X:
		DbgPrint("TOOL_X: %d\n", event->value);
		break;
#endif
#if defined(ABS_MT_TOOL_Y)
	case ABS_MT_TOOL_Y:
		DbgPrint("TOOL_Y: %d\n", event->value);
		break;
#endif
#if defined(ABS_MT_TOOL_TYPE)
	case ABS_MT_TOOL_TYPE:
		DbgPrint("TOOL_TYPE: %d\n", event->value);
		break;
#endif
#if defined(ABS_MT_BLOB_ID)
	case ABS_MT_BLOB_ID:
		DbgPrint("BLOB_ID: %d\n", event->value);
		break;
#endif
#if defined(ABS_MT_DISTANCE)
	case ABS_MT_DISTANCE:
		DbgPrint("DISTANCE: %d\n", event->value);
		break;
#endif
#if defined(ABS_MT_PALM)
	case ABS_MT_PALM:
		DbgPrint("PALM: %d\n", event->value);
		break;
#endif
	default:
#if defined(ABS_MT_COMPONENT)
		if (event->code == ABS_MT_COMPONENT) {
			DbgPrint("COMPONENT: %d\n", event->value);
			break;
		}
#endif
#if defined(ABS_MT_ANGLE)
		if (event->code == ABS_MT_ANGLE) {
			DbgPrint("ANGLE: %d\n", event->value);
			break;
		}
#endif
#if defined(ABS_MT_SUMSIZE)
		if (event->code == ABS_MT_SUMSIZE) {
			DbgPrint("SUMSIZE: %d\n", event->value);
			break;
		}
#endif
		break;
	}

	return;
}

static inline int processing_input_event(struct input_event *event)
{
	struct dlist *l;
	struct event_object *eo;

	if (s_info.timestamp_updated == 0) {
		update_timestamp(event);
	}

	switch (event->type) {
	case EV_SYN:
		switch (event->code) {
		case SYN_CONFIG:
			break;
		case SYN_MT_REPORT:
		case SYN_REPORT:
			s_info.timestamp_updated = 0;

			s_info.last_event_data.timestamp = s_info.event_data.tv;
			s_info.last_event_data.info.pointer.x = s_info.event_data.x;
			s_info.last_event_data.info.pointer.y = s_info.event_data.y;

			dlist_foreach(s_info.event_object_list, l, eo) {
				if (s_info.last_event_data.timestamp >= eo->timestamp) {
					struct widget_buffer_event_data evdata;

					DbgPrint("event input time: %lf, eo timestamp: %lf\n", s_info.event_data.tv, eo->timestamp);

					evdata.timestamp = s_info.last_event_data.timestamp;
					evdata.info.pointer.x = s_info.last_event_data.info.pointer.x - eo->x;
					evdata.info.pointer.y = s_info.last_event_data.info.pointer.y - eo->y;

					switch (eo->state) {
					case EVENT_STATE_ACTIVATE:
						eo->state = EVENT_STATE_ACTIVATED;
						provider_buffer_direct_mouse_down(eo->handler, &evdata);
						break;
					case EVENT_STATE_ACTIVATED:
						provider_buffer_direct_mouse_move(eo->handler, &evdata);
						break;
					case EVENT_STATE_DEACTIVATE:
					case EVENT_STATE_DEACTIVATED:
					default:
						// Will not be able to reach here
						break;
					}
				} else {
					DbgPrint("Discard old events: %lf < %lf\n", s_info.last_event_data.timestamp, eo->timestamp);
				}
			}
			break;
#if defined(SYN_DROPPED)
		case SYN_DROPPED:
			DbgPrint("EV_SYN, SYN_DROPPED\n");
			break;
#endif
		default:
			DbgPrint("EV_SYN, 0x%x\n", event->code);
			break;
		}
		break;
	case EV_KEY:
		DbgPrint("EV_KEY: 0x%X\n", event->value);
		s_info.event_data.keycode = event->value;
		break;
	case EV_REL:
		DbgPrint("EV_REL: 0x%X\n", event->value);
		break;
	case EV_ABS:
		processing_ev_abs(event);
		break;
	case EV_MSC:
	case EV_SW:
	case EV_LED:
	case EV_SND:
	case EV_REP:
	case EV_FF:
	case EV_PWR:
	case EV_FF_STATUS:
	default:
		DbgPrint("0x%X, 0x%X\n", event->type, event->code);
		break;
	}

	return WIDGET_ERROR_NONE;
}

static gboolean event_cb(GIOChannel *src, GIOCondition cond, gpointer data)
{
	int fd;
	int ret;
	char *ptr = (char *)&s_info.input_event;
	struct dlist *l;
	struct dlist *n;
	struct event_object *eo;

	fd = g_io_channel_unix_get_fd(src);
	if (!(cond & G_IO_IN)) {
		DbgPrint("FD is closed (%d - %d)\n", fd, s_info.fd);
		goto errout;
	}

	if ((cond & G_IO_ERR) || (cond & G_IO_HUP) || (cond & G_IO_NVAL)) {
		DbgPrint("FD is lost: %d - %d\n", fd, s_info.fd);
		goto errout;
	}

	ret = read(fd, ptr + s_info.readsize, sizeof(s_info.input_event) - s_info.readsize);
	if (ret < 0) {
		ErrPrint("read: %d: %d - %d\n", errno, fd, s_info.fd);
		goto errout;
	}

	s_info.readsize += ret;
	if (s_info.readsize == sizeof(s_info.input_event)) {
		s_info.readsize = 0;
		if (processing_input_event(&s_info.input_event) < 0) {
			goto errout;
		}
	}

	return TRUE;

errout:
	dlist_foreach_safe(s_info.event_object_list, l, n, eo) {
		struct widget_buffer_event_data evdata;

		s_info.event_object_list = dlist_remove(s_info.event_object_list, l);

		if (eo->state == EVENT_STATE_ACTIVATED || s_info.last_event_data.timestamp >= eo->timestamp) {
			evdata.timestamp = s_info.last_event_data.timestamp;
			evdata.info.pointer.x = s_info.last_event_data.info.pointer.x - eo->x;
			evdata.info.pointer.y = s_info.last_event_data.info.pointer.y - eo->y;

			switch (eo->state) {
			case EVENT_STATE_ACTIVATE:
				provider_buffer_direct_mouse_down(eo->handler, &evdata);
			case EVENT_STATE_ACTIVATED:
				provider_buffer_direct_mouse_move(eo->handler, &evdata);
			case EVENT_STATE_DEACTIVATE:
				provider_buffer_direct_mouse_up(eo->handler, &evdata);
			case EVENT_STATE_DEACTIVATED:
			default:
				break;
			}
		} else {
			ErrPrint("No events are pumped: 0x%X\n", eo->state);
		}

		free(eo);
	}

	if (close(fd) < 0) {
		ErrPrint("close: %d\n", errno);
	}

	if (s_info.fd == fd) {
		s_info.fd = -1;
	}
	return FALSE;
}

int event_add_object(int fd, widget_buffer_h handler, double timestamp, int x, int y)
{
	GIOChannel *gio;
	struct event_object *eo;
	struct dlist *l;

	if (fd < 0) {
		ErrPrint("Invalid parameter: %d\n", fd);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	dlist_foreach(s_info.event_object_list, l, eo) {
		if (eo->handler == handler) {
			ErrPrint("Already registered\n");
			return WIDGET_ERROR_ALREADY_EXIST;
		}
	}

	eo = malloc(sizeof(*eo));
	if (!eo) {
		ErrPrint("malloc: %d\n", errno);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	DbgPrint("[%lf] (%dx%d) is updated\n", timestamp, x, y);
	eo->timestamp = timestamp;
	eo->x = x;
	eo->y = y;
	eo->handler = handler;
	eo->state = EVENT_STATE_ACTIVATE;

	s_info.event_object_list = dlist_append(s_info.event_object_list, eo);

	if (s_info.fd >= 0) {
		DbgPrint("FD: %d is already allocated\nclose(%d)", s_info.fd, fd);
		if (s_info.fd != fd && close(fd) < 0) {
			ErrPrint("close: %d\n", errno);
		}
		return WIDGET_ERROR_ALREADY_STARTED;
	}

	gio = g_io_channel_unix_new(fd);
	if (!gio) {
		ErrPrint("Failed to create a gio\n");
		goto errout;
	}

	g_io_channel_set_close_on_unref(gio, FALSE);
	s_info.id = g_io_add_watch(gio, G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL, (GIOFunc)event_cb, NULL);
	if (s_info.id == 0) {
		GError *err = NULL;
		ErrPrint("Failed to add IO watch\n");
		g_io_channel_shutdown(gio, TRUE, &err);
		if (err) {
			ErrPrint("Shutdown: %s\n", err->message);
			g_error_free(err);
		}
		g_io_channel_unref(gio);
		goto errout;
	}
	g_io_channel_unref(gio);

	s_info.fd = fd;
	DbgPrint("FD(%d) is registered\n", fd);

	return WIDGET_ERROR_NONE;

errout:
	if (close(fd) < 0) {
		ErrPrint("close: %d\n", errno);
	}
	dlist_remove_data(s_info.event_object_list, eo);
	free(eo);
	return WIDGET_ERROR_FAULT;
}

int event_input_fd(void)
{
	return s_info.fd;
}

int event_remove_object(widget_buffer_h handler)
{
	struct dlist *l;
	struct dlist *n;
	struct event_object *eo;

	if (s_info.fd < 0) {
		ErrPrint("Invalid file descriptor\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	dlist_foreach_safe(s_info.event_object_list, l, n, eo) {
		if (eo->handler == handler) {
			struct widget_buffer_event_data evdata;

			s_info.event_object_list = dlist_remove(s_info.event_object_list, l);

			if (eo->state == EVENT_STATE_ACTIVATED || s_info.last_event_data.timestamp >= eo->timestamp) {
				evdata.timestamp = s_info.last_event_data.timestamp;
				evdata.info.pointer.x = s_info.last_event_data.info.pointer.x - eo->x;
				evdata.info.pointer.y = s_info.last_event_data.info.pointer.y - eo->y;

				switch (eo->state) {
				case EVENT_STATE_ACTIVATE:
					provider_buffer_direct_mouse_down(eo->handler, &evdata);
				case EVENT_STATE_ACTIVATED:
					provider_buffer_direct_mouse_move(eo->handler, &evdata);
				case EVENT_STATE_DEACTIVATE:
					provider_buffer_direct_mouse_up(eo->handler, &evdata);
				case EVENT_STATE_DEACTIVATED:
				default:
					break;
				}
			} else {
				DbgPrint("No events are pumped 0x%X\n", eo->state);
			}

			free(eo);
			break;
		}
	}

	if (s_info.event_object_list == NULL) {
		if (s_info.id > 0) {
			g_source_remove(s_info.id);
			s_info.id = 0;
		}

		DbgPrint("Close: %d\n", s_info.fd);
		if (close(s_info.fd) < 0) {
			ErrPrint("close: %d\n", errno);
		}

		s_info.fd = -1;
		return WIDGET_ERROR_NONE;
	}

	return WIDGET_ERROR_RESOURCE_BUSY;
}

/* End of a file */
