#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "config.h"
#include "my_math.h"
#include "suncalc.h"

#define MY_UUID {0xB9,0x51,0xEB,0x8C,0x14,0x58,0x4F,0x52,0x94,0x50,0x44,0x17,0xE0,0x5C,0xDE,0xD4}
PBL_APP_INFO(MY_UUID,
             "SunClock", "Boldo",
             1, 0, /* App version */
             RESOURCE_ID_IMAGE_MENU_ICON,
             APP_INFO_WATCH_FACE);

Window window;

TextLayer text_time_layer;
TextLayer text_sunrise_layer;
TextLayer text_sunset_layer;

Layer graphics_sun_layer;

RotBmpPairContainer bitmap_hour_container;
RotBmpPairContainer watchface_container;

GPathInfo sun_path_info = {
  5,
  (GPoint []) {
    {0, 0},
    {-73, +84}, //replaced by sunrise angle
    {-73, +84}, //bottom left
    {+73, +84}, //bottom right
    {+73, +84}, //replaced by sunset angle
  }
};

GPath sun_path;
short currentData = -1;
char moonphase_text [] = "a";  
GRect moonRect = {{(144/2)-16,100},{32,32}};
GFont fontSmall;
GFont fontBig;
GFont fontMoonPhases;

void graphics_sun_layer_update_callback(Layer *me, GContext* ctx) 
{
  (void)me;

  gpath_init(&sun_path, &sun_path_info);
  gpath_move_to(&sun_path, grect_center_point(&graphics_sun_layer.frame));

  graphics_context_set_fill_color(ctx, GColorBlack);
  gpath_draw_filled(ctx, &sun_path);  

  graphics_context_set_text_color(ctx, GColorWhite);

  graphics_text_draw(ctx,
         moonphase_text,
         fontMoonPhases,
         moonRect, //GRect(0, 100, 144, 50),
         GTextOverflowModeWordWrap,
         GTextAlignmentCenter,
         NULL);
}

float get24HourAngle(int hours, int minutes) 
{
  return (12.0f + hours + (minutes/60.0f)) / 24.0f;
}

void adjustTimezone(float* time) 
{
  *time += TIMEZONE;
  if (*time > 24) *time -= 24;
  if (*time < 0) *time += 24;
}

float gregorian_calendar_to_jd(int y, int m, int d)
{
    y+=8000;
    if(m<3) { y--; m+=12; }
    return ((y-9865)*365) +(y/4) -(y/100) +(y/400) - 95
    +(m*153+3)/5-92
    +d-1
    ;
}

float moon_phase(int y, int m, int d)
{
    static const int known_full_moon = 51550; //2000 1 6
    int julianDay = gregorian_calendar_to_jd(y,m,d);
    float phase = (julianDay - known_full_moon)/29.53f;
    phase = phase - (int)(phase);
    return phase;
}

void updateDayAndNightInfo()
{
  static char sunrise_text[] = "00:00";
  static char sunset_text[] = "00:00";
  

  PblTm pblTime;
  get_time(&pblTime);

  if(currentData != pblTime.tm_hour) 
  {
    moonphase_text[0] = 'a' + 26.0f * moon_phase(1900 + pblTime.tm_year,  1 + pblTime.tm_mon, pblTime.tm_mday);
  
    char *time_format;

    if (clock_is_24h_style()) 
    {
      time_format = "%R";
    } 
    else 
    {
      time_format = "%I:%M";
    }

    float sunriseTime = calcSunRise(pblTime.tm_year, pblTime.tm_mon+1, pblTime.tm_mday, LATITUDE, LONGITUDE, 91.0f);
    float sunsetTime = calcSunSet(pblTime.tm_year, pblTime.tm_mon+1, pblTime.tm_mday, LATITUDE, LONGITUDE, 91.0f);
    adjustTimezone(&sunriseTime);
    adjustTimezone(&sunsetTime);
    
    if (!pblTime.tm_isdst) 
    {
      sunriseTime+=1;
      sunsetTime+=1;
    } 
    
    pblTime.tm_min = (int)(60*(sunriseTime-((int)(sunriseTime))));
    pblTime.tm_hour = (int)sunriseTime;
    string_format_time(sunrise_text, sizeof(sunrise_text), time_format, &pblTime);
    text_layer_set_text(&text_sunrise_layer, sunrise_text);
    
    pblTime.tm_min = (int)(60*(sunsetTime-((int)(sunsetTime))));
    pblTime.tm_hour = (int)sunsetTime;
    string_format_time(sunset_text, sizeof(sunset_text), time_format, &pblTime);
    text_layer_set_text(&text_sunset_layer, sunset_text);
    text_layer_set_text_alignment(&text_sunset_layer, GTextAlignmentRight);

    sunriseTime+=12.0f;
    sun_path_info.points[1].x = (int16_t)(my_sin(sunriseTime/24 * M_PI * 2) * 120);
    sun_path_info.points[1].y = -(int16_t)(my_cos(sunriseTime/24 * M_PI * 2) * 120);

    sunsetTime+=12.0f;
    sun_path_info.points[4].x = (int16_t)(my_sin(sunsetTime/24 * M_PI * 2) * 120);
    sun_path_info.points[4].y = -(int16_t)(my_cos(sunsetTime/24 * M_PI * 2) * 120);
  
    currentData = pblTime.tm_hour;
  }
}

void handle_minute_tick(AppContextRef ctx, PebbleTickEvent *t) 
{
  (void)t;
  (void)ctx;

  // Need to be static because they're used by the system later.
  static char time_text[] = "00:00";

  char *time_format;

  if (clock_is_24h_style()) 
  {
    time_format = "%R";
  } 
  else 
  {
    time_format = "%I:%M";
  }

  string_format_time(time_text, sizeof(time_text), time_format, t->tick_time);

  if (!clock_is_24h_style() && (time_text[0] == '0')) 
  {
    memmove(time_text, &time_text[1], sizeof(time_text) - 1);
  }

  text_layer_set_text(&text_time_layer, time_text);
  text_layer_set_text_alignment(&text_time_layer, GTextAlignmentCenter);

  rotbmp_pair_layer_set_angle(&bitmap_hour_container.layer, TRIG_MAX_ANGLE * get24HourAngle(t->tick_time->tm_hour, t->tick_time->tm_min));
  bitmap_hour_container.layer.layer.frame.origin.x = (144/2) - (bitmap_hour_container.layer.layer.frame.size.w/2);
  bitmap_hour_container.layer.layer.frame.origin.y = (168/2) - (bitmap_hour_container.layer.layer.frame.size.h/2);

  updateDayAndNightInfo();
}


void handle_init(AppContextRef ctx) {
  (void)ctx;

  window_init(&window, "SunClock");
  window_stack_push(&window, true /* Animated */);
  window_set_background_color(&window, GColorWhite);

  resource_init_current_app(&APP_RESOURCES);

  fontBig = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SUNCLOCK_30));
  fontSmall = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SUNCLOCK_16));
  fontMoonPhases = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_MOON_PHASES_30));

  layer_init(&graphics_sun_layer, window.layer.frame);
  graphics_sun_layer.update_proc = &graphics_sun_layer_update_callback;
  layer_add_child(&window.layer, &graphics_sun_layer);

  rotbmp_pair_init_container(RESOURCE_ID_IMAGE_WATCHFACE_WHITE, RESOURCE_ID_IMAGE_WATCHFACE_BLACK, &watchface_container);
  layer_add_child(&graphics_sun_layer, &watchface_container.layer.layer);
  watchface_container.layer.layer.frame.origin.x = (144/2) - (watchface_container.layer.layer.frame.size.w/2);
  watchface_container.layer.layer.frame.origin.y = (168/2) - (watchface_container.layer.layer.frame.size.h/2);

  rotbmp_pair_init_container(RESOURCE_ID_IMAGE_HOUR_WHITE, RESOURCE_ID_IMAGE_HOUR_BLACK, &bitmap_hour_container);
  rotbmp_pair_layer_set_src_ic(&bitmap_hour_container.layer, GPoint(6,56));
  layer_add_child(&window.layer, &bitmap_hour_container.layer.layer);
  PblTm t;
  get_time(&t);
  rotbmp_pair_layer_set_angle(&bitmap_hour_container.layer, TRIG_MAX_ANGLE * get24HourAngle(t.tm_hour, t.tm_min));
  bitmap_hour_container.layer.layer.frame.origin.x = (144/2) - (bitmap_hour_container.layer.layer.frame.size.w/2);
  bitmap_hour_container.layer.layer.frame.origin.y = (168/2) - (bitmap_hour_container.layer.layer.frame.size.h/2);

  text_layer_init(&text_time_layer, window.layer.frame);
  text_layer_set_text_color(&text_time_layer, GColorBlack);
  text_layer_set_background_color(&text_time_layer, GColorClear);
  layer_set_frame(&text_time_layer.layer, GRect(0, 35, 144, 30));
  text_layer_set_font(&text_time_layer, fontBig);
  layer_add_child(&window.layer, &text_time_layer.layer);

  text_layer_init(&text_sunrise_layer, window.layer.frame);
  text_layer_set_text_color(&text_sunrise_layer, GColorWhite);
  text_layer_set_background_color(&text_sunrise_layer, GColorClear);
  layer_set_frame(&text_sunrise_layer.layer, GRect(0, 150, 144, 18));
  text_layer_set_font(&text_sunrise_layer, fontSmall);
  layer_add_child(&window.layer, &text_sunrise_layer.layer);

  text_layer_init(&text_sunset_layer, window.layer.frame);
  text_layer_set_text_color(&text_sunset_layer, GColorWhite);
  text_layer_set_background_color(&text_sunset_layer, GColorClear);
  layer_set_frame(&text_sunset_layer.layer, GRect(0, 150, 144, 18));
  text_layer_set_font(&text_sunset_layer, fontSmall);
  layer_add_child(&window.layer, &text_sunset_layer.layer); 

  updateDayAndNightInfo();
}

void handle_deinit(AppContextRef ctx) {
  (void)ctx;

  rotbmp_pair_deinit_container(&watchface_container);
  rotbmp_pair_deinit_container(&bitmap_hour_container);

  fonts_unload_custom_font(fontSmall);
  fonts_unload_custom_font(fontBig);
  fonts_unload_custom_font(fontMoonPhases);
}

void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .deinit_handler = &handle_deinit,

    .tick_info = {
      .tick_handler = &handle_minute_tick,
      .tick_units = MINUTE_UNIT
    }
  };
  app_event_loop(params, &handlers);
}
