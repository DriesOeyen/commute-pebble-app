#ifndef MAIN_H
#define MAIN_H

#include <pebble.h>



/**************************************************
 * DEFINES
 **************************************************/

#define LAYER_PAGE_INDICATOR_HEIGHT 16
#define LAYER_PAGE_INDICATOR_MARGIN 4



/**************************************************
 * TYPEDEFS
 **************************************************/

typedef enum {
	REQUEST_TYPE_LOCATION = 0,
	REQUEST_TYPE_HOME = 1,
	REQUEST_TYPE_WORK = 2
} RequestType;

typedef enum {
	RESPONSE_TYPE_READY = 0,
	RESPONSE_TYPE_LOCATED = 1,
	RESPONSE_TYPE_DIRECTIONS = 2,
	RESPONSE_TYPE_ERROR = 3,
	RESPONSE_TYPE_CONFIG_CHANGED = 4
} ResponseType;

typedef enum {
	REQUEST_ID = 0,
	REQUEST_ORIG = 1,
	REQUEST_DEST = 2,
	RESPONSE_TYPE = 3,
	RESPONSE_ERROR = 4,
	RESPONSE_DURATION_NORMAL = 5,
	RESPONSE_DURATION_TRAFFIC = 6,
	RESPONSE_VIA = 7
} AppMessageTupleType;

typedef enum {
	ERROR_TIMELINE_TOKEN = 0,
	ERROR_LOCATION = 1,
	ERROR_INTERNET_TIMEOUT = 2,
	ERROR_INTERNET_UNAVAILABLE = 3,
	ERROR_RESPONSE_UNEXPECTED = 4,
	ERROR_RESPONSE_NO_TRAFFIC_DATA = 5,
	ERROR_RESPONSE_ADDRESS_INCORRECT = 6,
	ERROR_RESPONSE_NO_ROUTE = 7,
	ERROR_CONFIGURE = 8,
	ERROR_RECONFIGURE = 9,
	ERROR_BLUETOOTH = 10
} Error;

typedef enum {
	STATUS_CONNECTING = 0,
	STATUS_LOCATING = 1,
	STATUS_FETCHING = 2,
	STATUS_DONE = 3,
	STATUS_ERROR = 4
} Status;

typedef enum {
	PAGE_LOCATION_WORK = 0,
	PAGE_LOCATION_HOME = 1,
	PAGE_HOME_WORK = 2,
	PAGE_WORK_HOME = 3
} Page;

typedef struct {
	Status status;
	Error error;
	int duration_current;
	int duration_delay;
	char via[16];
	bool mode_delay;
} DataLayerData;



/**************************************************
 * GLOBAL VARIABLES
 **************************************************/

Page page; // Keep track of the currently visible page
int request_id = -1; // Keep track of newest request

Window *window;

// Children of root layer
StatusBarLayer *status_bar_layer;
Layer *layer_page_indicator_up;
Layer *layer_page_indicator_down;
Layer *layer_data;

// Children of data layer
Layer *layer_page_icons;
TextLayer *layer_duration;
TextLayer *layer_duration_label;
TextLayer *layer_caption;
BitmapLayer *layer_status_icon;
GBitmap *icon_loading;
GBitmap *icon_error;
char string_duration[4];
char string_duration_label[14];
char string_caption[16];

// Children of page icon layer
BitmapLayer *layer_orig;
BitmapLayer *layer_to;
BitmapLayer *layer_dest;
GBitmap *icon_pin;
GBitmap *icon_home;
GBitmap *icon_work;
GBitmap *icon_arrow;

// Children of page indicator layers
BitmapLayer *layer_page_indicator_up_icon;
BitmapLayer *layer_page_indicator_down_icon;
GBitmap *icon_up;
GBitmap *icon_down;

#endif // MAIN_H
