#include <main.h>



/**************************************************
 * APPMESSAGE FUNCTIONS
 **************************************************/

// Write message to buffer & send
void send_request() {
	if(connection_service_peek_pebble_app_connection()) {
		// Bluetooth connected
		// Init message
		DictionaryIterator *iter;
		app_message_outbox_begin(&iter);

		// Prepare data
		request_id += 1;
		RequestType request_orig = -1;
		RequestType request_dest = -1;
		switch((Page) page) {
			case PAGE_LOCATION_WORK:
				request_orig = REQUEST_TYPE_LOCATION;
				request_dest = REQUEST_TYPE_WORK;
				break;
			case PAGE_LOCATION_HOME:
				request_orig = REQUEST_TYPE_LOCATION;
				request_dest = REQUEST_TYPE_HOME;
				break;
			case PAGE_HOME_WORK:
				request_orig = REQUEST_TYPE_HOME;
				request_dest = REQUEST_TYPE_WORK;
				break;
			case PAGE_WORK_HOME:
				request_orig = REQUEST_TYPE_WORK;
				request_dest = REQUEST_TYPE_HOME;
				break;
		}

		// Put data in tuples
		Tuplet tup_request_id = TupletInteger(REQUEST_ID, request_id);
		Tuplet tup_request_orig = TupletInteger(REQUEST_ORIG, request_orig);
		Tuplet tup_request_dest = TupletInteger(REQUEST_DEST, request_dest);

		// Put tuples in dictionary
		dict_write_tuplet(iter, &tup_request_id);
		dict_write_tuplet(iter, &tup_request_orig);
		dict_write_tuplet(iter, &tup_request_dest);

		// Send message
		app_message_outbox_send();
	} else {
		// No Bluetooth connection
		DataLayerData *data_layer_data = (DataLayerData*) layer_get_data(layer_data);
		data_layer_data->status = STATUS_ERROR;
		data_layer_data->error = ERROR_BLUETOOTH_DISCONNECTED;
		GColor color_background = PBL_IF_COLOR_ELSE(GColorBlack, GColorBlack);
		window_set_background_color(window, color_background);
		layer_mark_dirty(window_get_root_layer(window));
	}
}

// Initiate data refresh
static void refresh_data() {
	// Bluetooth connected
	DataLayerData *data_layer_data = (DataLayerData*) layer_get_data(layer_data);
	data_layer_data->status = STATUS_FETCHING;
	GColor color_background = PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack);
	window_set_background_color(window, color_background);
	layer_mark_dirty(window_get_root_layer(window));
	send_request();
}

// AppMessage callbacks
static void in_received_handler(DictionaryIterator *received, void *context) {
	#ifdef DEBUG
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Parsing incoming AppMessage...");
	#endif
	
	Tuple *tup_request_id = dict_find(received, REQUEST_ID);
	Tuple *tup_response_type = dict_find(received, RESPONSE_TYPE);
	Tuple *tup_response_error = dict_find(received, RESPONSE_ERROR);
	Tuple *tup_response_duration_normal = dict_find(received, RESPONSE_DURATION_NORMAL);
	Tuple *tup_response_duration_traffic = dict_find(received, RESPONSE_DURATION_TRAFFIC);
	Tuple *tup_response_via = dict_find(received, RESPONSE_VIA);
	
	DataLayerData *data_layer_data = (DataLayerData*) layer_get_data(layer_data);
	int duration_difference;
	float delay_ratio;
	GColor color_background;
	
	switch((ResponseType) tup_response_type->value->uint8) {
		case RESPONSE_TYPE_READY:
			data_layer_data->status = STATUS_LOCATING;
			color_background = PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack);
			window_set_background_color(window, color_background);
			layer_mark_dirty(window_get_root_layer(window));
			send_request();
			break;
		case RESPONSE_TYPE_LOCATED:
			data_layer_data->status = STATUS_FETCHING;
			color_background = PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack);
			window_set_background_color(window, color_background);
			layer_mark_dirty(window_get_root_layer(window));
			break;
		case RESPONSE_TYPE_DIRECTIONS:
			// Filter out outdated requests
			if(request_id == tup_request_id->value->uint8) {
				// Calculate delay
				duration_difference = tup_response_duration_traffic->value->uint16 - tup_response_duration_normal->value->uint16;
				if(duration_difference < 0) // Set delay to 0 if negative
					duration_difference = 0;
				delay_ratio = (float) duration_difference / (float) tup_response_duration_normal->value->uint16;
				// Update data layer data
				data_layer_data->status = STATUS_DONE;
				data_layer_data->duration_current = tup_response_duration_traffic->value->uint16;
				data_layer_data->duration_delay = duration_difference;
				snprintf(data_layer_data->via, sizeof(data_layer_data->via), "%s", tup_response_via->value->cstring);
				data_layer_data->mode_delay = false;
				// Set color
				if(delay_ratio > 0.25) { // Heavy delay
					color_background = PBL_IF_COLOR_ELSE(GColorDarkCandyAppleRed, GColorBlack);
				} else if(delay_ratio > 0.1) { // Moderate delay
					color_background = PBL_IF_COLOR_ELSE(GColorOrange, GColorBlack);
				} else { // Light delay
					color_background = PBL_IF_COLOR_ELSE(GColorDarkGreen, GColorBlack);
				}
				window_set_background_color(window, color_background);
				layer_mark_dirty(window_get_root_layer(window));
			} else {
				APP_LOG(APP_LOG_LEVEL_INFO, "Received response for old request, dropping.");
			}
			break;
		case RESPONSE_TYPE_ERROR:
			data_layer_data->status = STATUS_ERROR;
			data_layer_data->error = (Error) tup_response_error->value->int8;
			color_background = PBL_IF_COLOR_ELSE(GColorBlack, GColorBlack);
			window_set_background_color(window, color_background);
			layer_mark_dirty(window_get_root_layer(window));
			break;
		case RESPONSE_TYPE_CONFIG_CHANGED:
			refresh_data();
			break;
	}
}

static void in_dropped_handler(AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Dropping incoming AppMessage (error: %d)", reason);
	
	// Bluetooth transmission error
	DataLayerData *data_layer_data = (DataLayerData*) layer_get_data(layer_data);
	data_layer_data->status = STATUS_ERROR;
	data_layer_data->error = ERROR_BLUETOOTH_TRANSMISSION;
	GColor color_background = PBL_IF_COLOR_ELSE(GColorBlack, GColorBlack);
	window_set_background_color(window, color_background);
	layer_mark_dirty(window_get_root_layer(window));
}

static void out_sent_handler(DictionaryIterator *sent, void *context) {
	#ifdef DEBUG
	APP_LOG(APP_LOG_LEVEL_DEBUG, "PebbleKit JS ACK");
	#endif
}

static void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "PebbleKit JS NACK (error: %d)", reason);
	
	// Bluetooth transmission error
	DataLayerData *data_layer_data = (DataLayerData*) layer_get_data(layer_data);
	data_layer_data->status = STATUS_ERROR;
	data_layer_data->error = ERROR_BLUETOOTH_TRANSMISSION;
	GColor color_background = PBL_IF_COLOR_ELSE(GColorBlack, GColorBlack);
	window_set_background_color(window, color_background);
	layer_mark_dirty(window_get_root_layer(window));
}



/**************************************************
 * UI FUNCTIONS
 **************************************************/

// Page icon layer functions
static void draw_layer_page_icons() {
	// Draw the correct page icons
	switch((Page) page) {
		case PAGE_LOCATION_WORK:
			bitmap_layer_set_bitmap(layer_orig, icon_pin);
			bitmap_layer_set_bitmap(layer_dest, icon_work);
			break;
		case PAGE_LOCATION_HOME:
			bitmap_layer_set_bitmap(layer_orig, icon_pin);
			bitmap_layer_set_bitmap(layer_dest, icon_home);
			break;
		case PAGE_HOME_WORK:
			bitmap_layer_set_bitmap(layer_orig, icon_home);
			bitmap_layer_set_bitmap(layer_dest, icon_work);
			break;
		case PAGE_WORK_HOME:
			bitmap_layer_set_bitmap(layer_orig, icon_work);
			bitmap_layer_set_bitmap(layer_dest, icon_home);
			break;
	}
}

static void layer_page_icons_load() {
	// Create icon bitmaps
	icon_pin = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_PIN);
	icon_home = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_HOME);
	icon_work = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_WORK);
	icon_arrow = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ARROW);
	
	// Create icon layers
	layer_orig = bitmap_layer_create(GRect(0, 0, 20, 20));
	if(layer_orig == NULL)
		APP_LOG(APP_LOG_LEVEL_ERROR, "Couldn't create origin icon layer");
	bitmap_layer_set_compositing_mode(layer_orig, GCompOpSet);
	bitmap_layer_set_alignment(layer_orig, GAlignCenter);
	layer_add_child(layer_page_icons, bitmap_layer_get_layer(layer_orig));
	
	layer_to = bitmap_layer_create(GRect(25, 0, 20, 20));
	if(layer_to == NULL)
		APP_LOG(APP_LOG_LEVEL_ERROR, "Couldn't create to icon layer");
	bitmap_layer_set_compositing_mode(layer_to, GCompOpSet);
	bitmap_layer_set_alignment(layer_to, GAlignCenter);
	bitmap_layer_set_bitmap(layer_to, icon_arrow);
	layer_add_child(layer_page_icons, bitmap_layer_get_layer(layer_to));
	
	layer_dest = bitmap_layer_create(GRect(50, 0, 20, 20));
	if(layer_dest == NULL)
		APP_LOG(APP_LOG_LEVEL_ERROR, "Couldn't create destination icon layer");
	bitmap_layer_set_compositing_mode(layer_dest, GCompOpSet);
	bitmap_layer_set_alignment(layer_dest, GAlignCenter);
	layer_add_child(layer_page_icons, bitmap_layer_get_layer(layer_dest));
}

static void layer_page_icons_unload() {
	bitmap_layer_destroy(layer_orig);
	bitmap_layer_destroy(layer_to);
	bitmap_layer_destroy(layer_dest);
	
	gbitmap_destroy(icon_pin);
	gbitmap_destroy(icon_home);
	gbitmap_destroy(icon_work);
	gbitmap_destroy(icon_arrow);
}

// Data layer functions
static void draw_layer_data(Layer *layer, GContext *ctx) {
	// Prepare to redraw data layer
	layer_set_hidden(text_layer_get_layer(layer_duration), true);
	layer_set_hidden(text_layer_get_layer(layer_duration_label), true);
	layer_set_hidden(bitmap_layer_get_layer(layer_status_icon), true);
	DataLayerData *data_layer_data = (DataLayerData*) layer_get_data(layer_data);
	
	// Figure out what to draw
	switch(data_layer_data->status) {
		case STATUS_CONNECTING:
			bitmap_layer_set_bitmap(layer_status_icon, icon_loading);
			layer_set_hidden(bitmap_layer_get_layer(layer_status_icon), false);
			snprintf(string_caption, sizeof(string_caption), "Connecting...");
			break;
		case STATUS_LOCATING:
		case STATUS_FETCHING:
			bitmap_layer_set_bitmap(layer_status_icon, icon_loading);
			layer_set_hidden(bitmap_layer_get_layer(layer_status_icon), false);
			snprintf(string_caption, sizeof(string_caption), "Loading...");
			break;
		case STATUS_DONE:
			if(data_layer_data->mode_delay) {
				// Delay mode
				snprintf(string_duration, sizeof(string_duration), "%d", data_layer_data->duration_delay);
				snprintf(string_duration_label, sizeof(string_duration_label), "minute%c delay", (data_layer_data->duration_delay==1) ? ' ' : 's');
			} else {
				// Main mode
				snprintf(string_duration, sizeof(string_duration), "%d", data_layer_data->duration_current);
				snprintf(string_duration_label, sizeof(string_duration_label), "minute%c", (data_layer_data->duration_current==1) ? ' ' : 's');
			}
			layer_set_hidden(text_layer_get_layer(layer_duration), false);
			layer_set_hidden(text_layer_get_layer(layer_duration_label), false);
			snprintf(string_caption, sizeof(string_caption), "%s", data_layer_data->via);
			break;
		case STATUS_ERROR:
			bitmap_layer_set_bitmap(layer_status_icon, icon_error);
			layer_set_hidden(bitmap_layer_get_layer(layer_status_icon), false);
			switch((Error) data_layer_data->error) {
				case ERROR_TIMELINE_TOKEN:
					snprintf(string_caption, sizeof(string_caption), "Enable timeline");
					break;
				case ERROR_LOCATION:
					snprintf(string_caption, sizeof(string_caption), "No location");
					break;
				case ERROR_INTERNET_TIMEOUT:
					snprintf(string_caption, sizeof(string_caption), "No internet");
					break;
				case ERROR_INTERNET_UNAVAILABLE:
					snprintf(string_caption, sizeof(string_caption), "No internet");
					break;
				case ERROR_RESPONSE_UNEXPECTED:
					snprintf(string_caption, sizeof(string_caption), "Server error");
					break;
				case ERROR_RESPONSE_NO_TRAFFIC_DATA:
					snprintf(string_caption, sizeof(string_caption), "No traffic data");
					break;
				case ERROR_RESPONSE_ADDRESS_INCORRECT:
					snprintf(string_caption, sizeof(string_caption), "Check addresses");
					break;
				case ERROR_RESPONSE_NO_ROUTE:
					snprintf(string_caption, sizeof(string_caption), "No route found");
					break;
				case ERROR_CONFIGURE:
					snprintf(string_caption, sizeof(string_caption), "Configure on phone");
					break;
				case ERROR_RECONFIGURE:
					snprintf(string_caption, sizeof(string_caption), "Reconfigure app");
					break;
				case ERROR_BLUETOOTH_DISCONNECTED:
					snprintf(string_caption, sizeof(string_caption), "No Bluetooth");
					break;
				case ERROR_BLUETOOTH_TRANSMISSION:
					snprintf(string_caption, sizeof(string_caption), "Bluetooth error");
					break;
			}
			break;
	}
}

static void layer_data_load() {
	// Create icon bitmaps
	icon_loading = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_LOADING);
	icon_error = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ERROR);
	
	// Get data layer bounds
	GRect bounds_layer_data = layer_get_bounds(layer_data);
	
	// Set up page icons layer
	const GRect frame_page_icons = GRect(
		(bounds_layer_data.size.w / 2) - 35,
		LAYER_PAGE_INDICATOR_MARGIN,
		70,
		20
	);
	layer_page_icons = layer_create(frame_page_icons);
	if(layer_page_icons == NULL)
		APP_LOG(APP_LOG_LEVEL_ERROR, "Couldn't create page icons layer");
	layer_set_update_proc(layer_page_icons, draw_layer_page_icons);
	layer_page_icons_load();
	layer_add_child(layer_data, layer_page_icons);
	
	// Set up duration layer
	GRect frame_duration = GRect(
		0,
		(bounds_layer_data.size.h / 2) - 36,
		bounds_layer_data.size.w,
		42
	);
	layer_duration = text_layer_create(frame_duration);
	if(layer_duration == NULL)
		APP_LOG(APP_LOG_LEVEL_ERROR, "Couldn't create duration layer");
	text_layer_set_font(layer_duration, fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS));
	text_layer_set_text_color(layer_duration, GColorWhite);
	text_layer_set_background_color(layer_duration, GColorClear);
	text_layer_set_text_alignment(layer_duration, GTextAlignmentCenter);
	text_layer_set_text(layer_duration, string_duration);
	layer_add_child(layer_data, text_layer_get_layer(layer_duration));
	
	// Set up duration label layer
	GRect frame_duration_label = GRect(
		0,
		(bounds_layer_data.size.h / 2) + 4,
		bounds_layer_data.size.w,
		28
	);
	layer_duration_label = text_layer_create(frame_duration_label);
	if(layer_duration_label == NULL)
		APP_LOG(APP_LOG_LEVEL_ERROR, "Couldn't create duration label layer");
	text_layer_set_font(layer_duration_label, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text_color(layer_duration_label, GColorWhite);
	text_layer_set_background_color(layer_duration_label, GColorClear);
	text_layer_set_text_alignment(layer_duration_label, GTextAlignmentCenter);
	text_layer_set_text(layer_duration_label, string_duration_label);
	layer_add_child(layer_data, text_layer_get_layer(layer_duration_label));
	
	// Set up caption layer
	GRect frame_caption = GRect(
		0,
		bounds_layer_data.size.h - 22 - LAYER_PAGE_INDICATOR_MARGIN,
		bounds_layer_data.size.w,
		22
	);
	layer_caption = text_layer_create(frame_caption);
	if(layer_caption == NULL)
		APP_LOG(APP_LOG_LEVEL_ERROR, "Couldn't create caption layer");
	text_layer_set_font(layer_caption, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	text_layer_set_text_color(layer_caption, GColorWhite);
	text_layer_set_background_color(layer_caption, GColorClear);
	text_layer_set_text_alignment(layer_caption, GTextAlignmentCenter);
	text_layer_set_text(layer_caption, string_caption);
	layer_add_child(layer_data, text_layer_get_layer(layer_caption));
	
	// Set up status icon layer
	GRect frame_status_icon = GRect(
		(bounds_layer_data.size.w / 2) - 25,
		(bounds_layer_data.size.h / 2) - 25,
		50,
		50
	);
	layer_status_icon = bitmap_layer_create(frame_status_icon);
	if(layer_status_icon == NULL)
		APP_LOG(APP_LOG_LEVEL_ERROR, "Couldn't create status icon layer");
	bitmap_layer_set_compositing_mode(layer_status_icon, GCompOpSet);
	bitmap_layer_set_alignment(layer_status_icon, GAlignCenter);
	layer_add_child(layer_data, bitmap_layer_get_layer(layer_status_icon));
}

static void layer_data_unload() {
	layer_page_icons_unload();
	
	layer_destroy(layer_page_icons);
	text_layer_destroy(layer_duration);
	text_layer_destroy(layer_duration_label);
	text_layer_destroy(layer_caption);
	bitmap_layer_destroy(layer_status_icon);
	
	gbitmap_destroy(icon_loading);
	gbitmap_destroy(icon_error);
}

// Page indicator up layer functions
static void draw_layer_page_indicator_up() {
	// Draw the correct page icons
	if(page == PAGE_LOCATION_WORK) {
		layer_set_hidden(bitmap_layer_get_layer(layer_page_indicator_up_icon), true);
	} else {
		layer_set_hidden(bitmap_layer_get_layer(layer_page_indicator_up_icon), false);
	}
}

static void layer_page_indicator_up_load() {
	// Create icon bitmaps
	icon_up = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_UP);
	
	// Get data layer bounds
	GRect bounds_layer_page_indicator_up = layer_get_bounds(layer_page_indicator_up);
	
	// Create icon layers
	GRect frame_page_indicator_up_icon = GRect(
		0,
		0,
		bounds_layer_page_indicator_up.size.w,
		bounds_layer_page_indicator_up.size.h
	);
	layer_page_indicator_up_icon = bitmap_layer_create(frame_page_indicator_up_icon);
	if(layer_page_indicator_up_icon == NULL)
		APP_LOG(APP_LOG_LEVEL_ERROR, "Couldn't create page indicator up icon layer");
	bitmap_layer_set_compositing_mode(layer_page_indicator_up_icon, GCompOpSet);
	bitmap_layer_set_alignment(layer_page_indicator_up_icon, GAlignCenter);
	bitmap_layer_set_bitmap(layer_page_indicator_up_icon, icon_up);
	layer_add_child(layer_page_indicator_up, bitmap_layer_get_layer(layer_page_indicator_up_icon));
}

static void layer_page_indicator_up_unload() {
	bitmap_layer_destroy(layer_page_indicator_up_icon);
	gbitmap_destroy(icon_up);
}

// Page indicator down layer functions
static void draw_layer_page_indicator_down() {
	// Draw the correct page icons
	if(page == PAGE_WORK_HOME) {
		layer_set_hidden(bitmap_layer_get_layer(layer_page_indicator_down_icon), true);
	} else {
		layer_set_hidden(bitmap_layer_get_layer(layer_page_indicator_down_icon), false);
	}
}

static void layer_page_indicator_down_load() {
	// Create icon bitmaps
	icon_down = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_DOWN);
	
	// Get data layer bounds
	GRect bounds_layer_page_indicator_down = layer_get_bounds(layer_page_indicator_down);
	
	// Create icon layers
	GRect frame_page_indicator_down_icon = GRect(
		0,
		0,
		bounds_layer_page_indicator_down.size.w,
		bounds_layer_page_indicator_down.size.h
	);
	layer_page_indicator_down_icon = bitmap_layer_create(frame_page_indicator_down_icon);
	if(layer_page_indicator_down_icon == NULL)
		APP_LOG(APP_LOG_LEVEL_ERROR, "Couldn't create page indicator down icon layer");
	bitmap_layer_set_compositing_mode(layer_page_indicator_down_icon, GCompOpSet);
	bitmap_layer_set_alignment(layer_page_indicator_down_icon, GAlignCenter);
	bitmap_layer_set_bitmap(layer_page_indicator_down_icon, icon_down);
	layer_add_child(layer_page_indicator_down, bitmap_layer_get_layer(layer_page_indicator_down_icon));
}

static void layer_page_indicator_down_unload() {
	bitmap_layer_destroy(layer_page_indicator_down_icon);
	gbitmap_destroy(icon_down);
}

// Window layer functions
static void window_load(Window *window) {
	Layer *layer_window = window_get_root_layer(window);
	GRect bounds_window = layer_get_bounds(layer_window);
	
	// Set up status bar layer
	status_bar_layer = status_bar_layer_create();
	if(status_bar_layer == NULL)
		APP_LOG(APP_LOG_LEVEL_ERROR, "Couldn't create status bar layer");
	layer_add_child(layer_window, status_bar_layer_get_layer(status_bar_layer));
	status_bar_layer_set_colors(status_bar_layer, GColorClear, GColorWhite);
	status_bar_layer_set_separator_mode(status_bar_layer, StatusBarLayerSeparatorModeDotted);
	
	// Set up data layer
	const GRect frame_data = GRect(
		0,
		STATUS_BAR_LAYER_HEIGHT + LAYER_PAGE_INDICATOR_HEIGHT,
		bounds_window.size.w,
		bounds_window.size.h - STATUS_BAR_LAYER_HEIGHT - 2 * LAYER_PAGE_INDICATOR_HEIGHT - LAYER_PAGE_INDICATOR_MARGIN
	);
	layer_data = layer_create_with_data(frame_data, sizeof(DataLayerData));
	if(layer_data == NULL)
		APP_LOG(APP_LOG_LEVEL_ERROR, "Couldn't create data layer");
	DataLayerData *data_layer_data = (DataLayerData*) layer_get_data(layer_data);
	if(connection_service_peek_pebble_app_connection()) {
		// Bluetooth connected
		data_layer_data->status = STATUS_CONNECTING;
	} else {
		// No Bluetooth connection
		data_layer_data->status = STATUS_ERROR;
		data_layer_data->error = ERROR_BLUETOOTH_DISCONNECTED;
		GColor color_background = PBL_IF_COLOR_ELSE(GColorBlack, GColorBlack);
		window_set_background_color(window, color_background);
	}
	//layer_set_clips(layer_data, false);
	layer_set_update_proc(layer_data, draw_layer_data);
	layer_data_load();
	layer_add_child(layer_window, layer_data);
	
	// Set up page indicator up layer
	const GRect frame_page_indicator_up = GRect(
		0,
		STATUS_BAR_LAYER_HEIGHT,
		bounds_window.size.w,
		LAYER_PAGE_INDICATOR_HEIGHT
	);
	layer_page_indicator_up = layer_create(frame_page_indicator_up);
	if(layer_page_indicator_up == NULL)
		APP_LOG(APP_LOG_LEVEL_ERROR, "Couldn't create page indicator up layer");
	layer_set_update_proc(layer_page_indicator_up, draw_layer_page_indicator_up);
	layer_page_indicator_up_load();
	layer_add_child(layer_window, layer_page_indicator_up);
	
	// Set up page indicator down layer
	const GRect frame_page_indicator_down = GRect(
		0,
		bounds_window.size.h - LAYER_PAGE_INDICATOR_HEIGHT - LAYER_PAGE_INDICATOR_MARGIN,
		bounds_window.size.w,
		LAYER_PAGE_INDICATOR_HEIGHT
	);
	layer_page_indicator_down = layer_create(frame_page_indicator_down);
	if(layer_page_indicator_down == NULL)
		APP_LOG(APP_LOG_LEVEL_ERROR, "Couldn't create page indicator down layer");
	layer_set_update_proc(layer_page_indicator_down, draw_layer_page_indicator_down);
	layer_page_indicator_down_load();
	layer_add_child(layer_window, layer_page_indicator_down);
}

static void window_unload(Window *window) {
	layer_data_unload();
	layer_page_indicator_up_unload();
	layer_page_indicator_down_unload();
	
	status_bar_layer_destroy(status_bar_layer);
	layer_destroy(layer_data);
	layer_destroy(layer_page_indicator_up);
	layer_destroy(layer_page_indicator_down);
}



/**************************************************
 * CLICK HANDLER FUNCTIONS
 **************************************************/

static void click_handler_up(ClickRecognizerRef recognizer, void *context) {
	if(page > PAGE_LOCATION_WORK) {
		page -= 1;
		refresh_data();
	}
}

static void click_handler_select(ClickRecognizerRef recognizer, void *context) {
	DataLayerData *data_layer_data = (DataLayerData*) layer_get_data(layer_data);
	if(data_layer_data->status == STATUS_DONE) {
		data_layer_data->mode_delay = !data_layer_data->mode_delay;
		layer_mark_dirty(window_get_root_layer(window));
	} else {
		// If there's a problem, set Select to refresh
		refresh_data();
	}
}

static void click_handler_down(ClickRecognizerRef recognizer, void *context) {
	if(page < PAGE_WORK_HOME) {
		page += 1;
		refresh_data();
	}
}

static void click_config_provider(void *context) {
	window_single_click_subscribe(BUTTON_ID_UP, click_handler_up);
	window_single_click_subscribe(BUTTON_ID_SELECT, click_handler_select);
	window_single_click_subscribe(BUTTON_ID_DOWN, click_handler_down);
}



/**************************************************
 * APP LIFECYCLE FUNCTIONS
 **************************************************/

static void init(void) {
	// Determine the initial page
	char am_pm[3];
	time_t now = time(NULL);
	strftime(am_pm, sizeof(am_pm), "%p", localtime(&now));
	if(strcmp(am_pm, "AM") == 0)
		page = PAGE_LOCATION_WORK;
	else
		page = PAGE_LOCATION_HOME;
	
	// Create window
	window = window_create();
	if(window == NULL)
		APP_LOG(APP_LOG_LEVEL_ERROR, "Couldn't create window");
	GColor color_background = PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack);
	window_set_background_color(window, color_background);
	window_set_click_config_provider(window, click_config_provider);
	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload
	});
	window_stack_push(window, true);

	// Register AppMessage handlers
	app_message_register_inbox_received(in_received_handler);
	app_message_register_inbox_dropped(in_dropped_handler);
	app_message_register_outbox_sent(out_sent_handler);
	app_message_register_outbox_failed(out_failed_handler);
	
	app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

static void deinit(void) {
	app_message_deregister_callbacks();
	window_destroy(window);
}

int main( void ) {
	init();
	app_event_loop();
	deinit();
}
