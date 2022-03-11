
enum event_state {
    EVENT_STATE_DEACTIVATED,
    EVENT_STATE_ACTIVATE,
    EVENT_STATE_ACTIVATED,
    EVENT_STATE_DEACTIVATE,
    EVENT_STATE_ERROR
};

struct event_data {
    int x;
    int y;
    unsigned int keycode;
    int device;
    int slot;
    struct {
	int major;
	int minor;
    } touch;
    struct {
	int major;
	int minor;
    } width;
    int distance;	/* Hovering */
    int orientation;
    int pressure;
    int updated; /* Timestamp is updated */
    double tv;
};

extern int event_add_object(int fd, widget_buffer_h handler, double timestamp, int x, int y);
extern int event_remove_object(widget_buffer_h handler);
extern int event_input_fd(void);

/* End of a file */
