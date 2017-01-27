#include <pebble.h>
#include <time.h>
#include <stdio.h>

#define PERSIST_KEY_COUNT 255
#define PERSIST_KEY_BASE 32
#define MARKER_SIZE 4

#define HI_FROM_WATCH 9
#define HI_FROM_PHONE 10

#define CONN_TO_WATCH 11
#define DISCONN_TO_WATCH 12
#define ACK_TO_WATCH 13
#define NACK_TO_WATCH 14
#define KEY_TO_WATCH 15

#define KEY_DATA_TO_PHONE 32
#define KEY_COUNT_TO_PHONE 255

static const GRect BATTERY_POSITIONS =  { { 0,  0 }, { /* width: */ 144, /* height: */  16} };
static const GRect TIME_POSITIONS =     { { 0, 24 }, { /* width: */ 144, /* height: */  56} };
static const GRect DATE_POSITIONS =     { { 0,96 }, { /* width: */ 144, /* height: */  56} };
static const GRect TEXT_POSITIONS  =    { { 0,152 }, { /* width: */ 144, /* height: */  24} };
// total dimension: 176 (row) x 144 (col)


static int marker_count = 0;
static uint32_t marker_data[32];

static DictionaryIterator s_iterator;
static DictionaryIterator s_iterator_hi;
static bool app_conn_flag;
static bool connection_flag;
static bool acknowledged_flag;
static bool sent_flag;
static bool hi_flag;

static Window *s_main_window;
static ActionBarLayer *s_action_bar_layer;
static TextLayer *s_battery_layer;
static TextLayer *s_date_layer;
static TextLayer *s_time_layer;
static TextLayer *s_text_layer;
static char agi_text [32];

static uint8_t dummy_count = 0;


void delay_time(void){
  uint16_t t = (2 ^ 16) - 1;
  while(t>0){t--;}
}


static void battery_handler(BatteryChargeState battery_charge) {
  static char battery_text[] = "----";

  if (battery_charge.is_charging) {
    snprintf(battery_text, sizeof(battery_text), ">>>>");
  } else {
    snprintf(battery_text, sizeof(battery_text), "%d%%", battery_charge.charge_percent);
  }
  text_layer_set_text(s_battery_layer, battery_text);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {  
  time_t temp = time(NULL); 
  struct tm *temp_time = localtime(&temp);
  
  static char s_buffer_1[32];
  strftime(s_buffer_1, sizeof(s_buffer_1), "%I:%M", temp_time); //clock_is_24h_style()?"%H:%M":"%I:%M %p"
  text_layer_set_text(s_time_layer,s_buffer_1+((s_buffer_1[0]=='0')?1:0));
  //text_layer_set_text(s_time_layer, s_buffer_1);
  
  static char s_buffer_2[32];
  strftime(s_buffer_2, sizeof(s_buffer_2), "%B %d\n%A", temp_time); //%Y
  
  text_layer_set_text(s_date_layer, s_buffer_2);
  
  if (dummy_count==0){
    text_layer_set_text(s_text_layer, " ");
  }
  else{
    dummy_count--;
  }
    
}

static void bt_handler(bool connected) {
  
  if (connected) {
	  connection_flag = true;    
    //APP_LOG(APP_LOG_LEVEL_INFO, "Connected!");
	  delay_time();
  }
  else{
    //APP_LOG(APP_LOG_LEVEL_INFO, "Disconnected!");
    connection_flag = false;
    app_conn_flag = false;
	  hi_flag = false;
  }
}


static void say_hi_to_phone(){
  if ((!app_conn_flag) && hi_flag){
    DictionaryIterator *iterator = &s_iterator_hi;
    app_message_outbox_begin(&iterator);
    uint32_t key = (uint32_t) HI_FROM_WATCH;
    uint32_t value = (uint32_t) HI_FROM_WATCH;
    dict_write_uint32(iterator, key, value);
    
    delay_time();
    delay_time();
    delay_time();
    app_message_outbox_send();
    delay_time();
    delay_time();
    delay_time();
    
    //APP_LOG(APP_LOG_LEVEL_INFO, "Hi, Phone!");
    delay_time();
	  hi_flag = false;
  }
}

static bool send_condition_check(void){
  bool result = false;
  if ((marker_count>0) && connection_flag && app_conn_flag){
    result = true;
    //APP_LOG(APP_LOG_LEVEL_INFO, "Send condition TRUE!");
  }
  else{
    //APP_LOG(APP_LOG_LEVEL_INFO, "Send condition FALSE!");
  }
  return result;
}

static void send_data(void){
  if (marker_count>0){  
    DictionaryIterator *iter = &s_iterator;
    app_message_outbox_begin(&iter);    
    uint32_t key;
    uint32_t value;
    key = (uint32_t) KEY_COUNT_TO_PHONE;
    value = (uint32_t) marker_count;
    dict_write_uint32(iter, key, value);
    for (int k = 0; k < marker_count; k++){
      key = (uint32_t)(KEY_DATA_TO_PHONE + k);
      value = (uint32_t) marker_data[k];
      dict_write_uint32(iter, key, value);
      // APP_LOG(APP_LOG_LEVEL_INFO, "Sending %lu!", value);  
    }
    delay_time();
    delay_time();
    delay_time();
    app_message_outbox_send();
    delay_time();
    delay_time();
    delay_time();
    // APP_LOG(APP_LOG_LEVEL_INFO, "Sending %d Message!", marker_count);
  }
}

static bool clear_condition_check(void){
  bool result = false;
  if ((marker_count>0) && sent_flag && acknowledged_flag){
    result = true;
    //APP_LOG(APP_LOG_LEVEL_INFO, "Clear condition TRUE!");
  }
  else{
    //APP_LOG(APP_LOG_LEVEL_INFO, "Clear condition FALSE!");
  }
  return result;
}

static void clear_marker(void){  
  if (marker_count>0){
    memset(&marker_data[0], 0, sizeof(marker_data));    
    marker_count = 0;
    //APP_LOG(APP_LOG_LEVEL_INFO, "Clearing Marker!");
    sent_flag = false;
    acknowledged_flag = false;
  }  
}

static void update_agi(void) {     
  time_t now = time( NULL );
  marker_count = marker_count + 1;
  marker_data[marker_count-1] = now;

  
  // New Display // changes made: 03/20/16
  text_layer_set_text(s_text_layer, "Event Marked!");
  dummy_count = 1;
  
  if (send_condition_check()){    
    send_data();
  }
}

static void single_click_handler(ClickRecognizerRef recognizer, void *context) {
  update_agi();
}

static void back_click_handler(ClickRecognizerRef recognizer, void *context) {
  update_agi();
  delay_time();
}

static void long_click_handler(ClickRecognizerRef recognizer, void *context) {
  delay_time();
}

static void multi_click_handler(ClickRecognizerRef recognizer, void *context) {
  delay_time();
}

static void config_provider(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, single_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, single_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, single_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click_handler);
  window_long_click_subscribe(BUTTON_ID_UP, 1000, long_click_handler, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 1000, long_click_handler, NULL);
  window_long_click_subscribe(BUTTON_ID_SELECT, 1000, long_click_handler, NULL);
  window_multi_click_subscribe(BUTTON_ID_UP, 2, 3, 1000, true, multi_click_handler);
  window_multi_click_subscribe(BUTTON_ID_DOWN, 2, 3, 1000, true, multi_click_handler);
  window_multi_click_subscribe(BUTTON_ID_SELECT, 2, 3, 1000, true, multi_click_handler);
  window_multi_click_subscribe(BUTTON_ID_BACK, 2, 3, 1000, true, multi_click_handler);
}


static void inbox_received_callback(DictionaryIterator *iter, void *context) {
  // Get the data pair
  Tuple *received_data = dict_find(iter, KEY_TO_WATCH);
  if (received_data) {
    uint32_t flag = (uint32_t)received_data->value->uint32;
    if (flag == (uint32_t) HI_FROM_PHONE){
      //APP_LOG(APP_LOG_LEVEL_INFO, "KEY_TO_WATCH received with value %lu", flag);
      //APP_LOG(APP_LOG_LEVEL_INFO, "Received HI from Phone!");   
	    hi_flag = true;
      say_hi_to_phone();      
    }    
    else if (flag == (uint32_t) CONN_TO_WATCH){
      //APP_LOG(APP_LOG_LEVEL_INFO, "KEY_TO_WATCH received with value %lu", flag);
      app_conn_flag = true;
      //APP_LOG(APP_LOG_LEVEL_INFO, "App Connection Confirmation!");    
      //bt_handler(connection_service_peek_pebble_app_connection());
      
      if (send_condition_check()){    
        send_data();
      }
      //connection_flag = 1;
    }
    else if (flag == (uint32_t) DISCONN_TO_WATCH){
      //APP_LOG(APP_LOG_LEVEL_INFO, "KEY_TO_WATCH received with value %lu", flag);
      app_conn_flag = false;
      hi_flag = false;
    }
    else if (flag == (uint32_t) ACK_TO_WATCH){
      //APP_LOG(APP_LOG_LEVEL_INFO, "KEY_TO_WATCH received with value %lu", flag);
      acknowledged_flag = true;
      delay_time();
      if (clear_condition_check()){
        clear_marker();
      }
    }
    else if (flag == (uint32_t) NACK_TO_WATCH){
      //APP_LOG(APP_LOG_LEVEL_INFO, "KEY_TO_WATCH received with value %lu", flag);
      acknowledged_flag = false;
      //bt_handler(connection_service_peek_pebble_app_connection());
      if (send_condition_check()){    
        send_data();
      }
    }
    
  } else {
    //APP_LOG(APP_LOG_LEVEL_INFO, "KEY_TO_WATCH not received.");
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  //APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  //APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
  sent_flag = false;
  if (send_condition_check()){    
    send_data();
  }
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  //APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
  sent_flag = true;  
}


static void app_msg_init(void){
  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  // Open AppMessage with sensible buffer sizes
  app_message_open(128, 256);
}

static void load_markers() {
  // Count number of markers
  marker_count = 0;
  // Check to see if a count already exists
  if (persist_exists(PERSIST_KEY_COUNT)) {
    // Load stored count
    marker_count = persist_read_int(PERSIST_KEY_COUNT);
    persist_delete(PERSIST_KEY_COUNT);
    
    // Read markers if exist
    if (marker_count>0){
      uint32_t data_t;
      for (int k = 0; k< marker_count; k++){
        uint32_t PERSIST_KEY_DATA = (uint32_t)PERSIST_KEY_BASE + (uint32_t)k;
        if (persist_exists(PERSIST_KEY_DATA)) {
          // Load stored markers
          persist_read_data(PERSIST_KEY_DATA, &data_t, MARKER_SIZE);
          persist_delete(PERSIST_KEY_DATA);
          marker_data[k] = data_t;
        } 
      }
    }
  }
  //APP_LOG(APP_LOG_LEVEL_INFO, "Loaded Markers!");  
}

static void save_markers() {
  // Save count of markers
  if (marker_count>0){
    persist_write_int(PERSIST_KEY_COUNT, marker_count);
    uint32_t data_t;
    // Save stored markers
    for (int k = 0; k< marker_count; k++){
      data_t = marker_data[k];
      uint32_t PERSIST_KEY_DATA = (uint32_t)PERSIST_KEY_BASE + (uint32_t)k;
      persist_write_data(PERSIST_KEY_DATA, &data_t, MARKER_SIZE);
    }
    marker_count = 0;
    memset(&marker_data[0], 0, sizeof(marker_data));
  }
  //APP_LOG(APP_LOG_LEVEL_INFO, "Saved Markers!");
}

static void main_window_unload(Window *window) {
  action_bar_layer_destroy(s_action_bar_layer);
  text_layer_destroy(s_battery_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_text_layer);
  // text_layer_destroy(agi_text_layer);   // changes made: 03/20/16
  // fonts_unload_custom_font(s_date_font);  
  //fonts_unload_custom_font(s_time_font);
}

static void main_window_load(Window *window) {  
  // Create ActionBar
  s_action_bar_layer = action_bar_layer_create();
  action_bar_layer_add_to_window(s_action_bar_layer, window);
  action_bar_layer_set_click_config_provider(s_action_bar_layer, config_provider);
  
  // Create TextLayer for Battery
  s_battery_layer = text_layer_create(BATTERY_POSITIONS);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_battery_layer));
  text_layer_set_background_color(s_battery_layer, GColorBlack);
  text_layer_set_text_color(s_battery_layer, GColorWhite);
  text_layer_set_text_alignment(s_battery_layer, GTextAlignmentRight);
  text_layer_set_font(s_battery_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  
  // Create TextLayer for Time
  s_time_layer = text_layer_create(TIME_POSITIONS);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer));
  text_layer_set_text(s_time_layer, "00:00");
  text_layer_set_background_color(s_time_layer, GColorBlack);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49)); //FONT_KEY_ROBOTO_BOLD_SUBSET_49

  // Create TextLayer for Date
  s_date_layer = text_layer_create(DATE_POSITIONS);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_date_layer));
  text_layer_set_text(s_date_layer, "XXXDay\n00/00/0000");
  text_layer_set_background_color(s_date_layer, GColorBlack);
  text_layer_set_text_color(s_date_layer, GColorWhite);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  
  // Create TextLayer for Text
  s_text_layer = text_layer_create(TEXT_POSITIONS);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_text_layer));
  text_layer_set_text(s_text_layer, " "); //"No event marked" // changes made: 03/20/16
  text_layer_set_background_color(s_text_layer, GColorBlack);
  text_layer_set_text_color(s_text_layer, GColorWhite);
  text_layer_set_text_alignment(s_text_layer, GTextAlignmentCenter);
  text_layer_set_font(s_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14)); 

}

static void init() {  
  // Create Window
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  //window_set_fullscreen(s_main_window, true);  
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);
  
  // Subscribe to Timer service
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  // Subscribe to Battery service
  battery_state_service_subscribe(battery_handler);
  
  // Load Persist Data
  load_markers();
  
  // App Message
  app_msg_init();
   
  // Subscribe to Bluetooth updates
  connection_service_subscribe((ConnectionHandlers) {
     .pebble_app_connection_handler = bt_handler
  });

  app_conn_flag = false;
  connection_flag = false;
  acknowledged_flag = false;
  sent_flag = false;
  hi_flag = false;
  
  
  time_t now = time( NULL );
  struct tm *timeinfo = localtime ( &now );
  strftime (agi_text, sizeof(agi_text),"since %X\n on %x", timeinfo);
  if (marker_count>0){
    //text_layer_set_text(s_text_layer, "Last event marked"); // changes made: 03/20/16
    now = (time_t) marker_data[marker_count-1];
    timeinfo = localtime ( &now );
    strftime (agi_text, sizeof(agi_text),"at %X\n on %x", timeinfo); 
  }
  
  
  if (connection_service_peek_pebble_app_connection()){
    connection_flag = true;
    //APP_LOG(APP_LOG_LEVEL_INFO, "Wake Up!");
	  delay_time();
    delay_time();
    delay_time();
    
 	  hi_flag = true;
 	  say_hi_to_phone();
  }
  
  battery_handler(battery_state_service_peek());

}

static void deinit() {
  // Persist Data Storage
  save_markers();
  // Clock Timer
  tick_timer_service_unsubscribe();
  // Battery Charge State
  battery_state_service_unsubscribe();
  // Bluetooth Connection
  connection_service_unsubscribe();
  // App Message
  app_message_deregister_callbacks();
  // Destroy Window
  window_destroy(s_main_window);
}


int main(void) {  
  init();
  app_event_loop();
  deinit();
}