#include <stdio.h>
#include <widget_errno.h>

static struct info {
	int manual_sync;
	int frame_drop_for_resizing;
	int shared_content;
	int direct_update;
	int extra_buffer_count;

	double event_filter;
} s_info = {
	.manual_sync = 0,
	.frame_drop_for_resizing = 1,
	.shared_content = 0,
	.direct_update = 0,
	.extra_buffer_count = 0,

	.event_filter = 0.01f,
};

void conf_set_direct_update(int flag)
{
	s_info.direct_update = flag;
}

int conf_direct_update(void)
{
	return s_info.direct_update;
}

void conf_set_manual_sync(int flag)
{
	s_info.manual_sync = flag;
}

int conf_manual_sync(void)
{
	return s_info.manual_sync;
}

void conf_set_frame_drop_for_resizing(int flag)
{
	s_info.frame_drop_for_resizing = flag;
}

int conf_frame_drop_for_resizing(void)
{
	return s_info.frame_drop_for_resizing;
}

void conf_set_shared_content(int flag)
{
	s_info.shared_content = flag;
}

int conf_shared_content(void)
{
	return s_info.shared_content;
}

double conf_event_filter(void)
{
	return s_info.event_filter;
}

void conf_set_event_filter(double filter)
{
	s_info.event_filter = filter;
}

void conf_set_extra_buffer_count(int buffer_count)
{
	s_info.extra_buffer_count = buffer_count;
}

int conf_extra_buffer_count(void)
{
	return s_info.extra_buffer_count;
}

/* End of a file */
