#include "watch1.h"
#include "MyInclude.h"
#include <strings.h>
#include <string.h>
#include <json-glib/json-glib.h> //JSON 파싱을 위한 라이브러리 헤더파일
#include <curl/curl.h> //CURL 라이브러리 헤더파일
#define WATCH_RADIUS 180
#define TEXT_BUF_SIZE 256

//json형식의 날씨 api
char url[200] = "http://api.openweathermap.org/data/2.5/forecast/daily?&APPID=735af42a432d8115ad400ba142e12b34&lat=37.335887&lon=126.584063&mode=json&units=metric&cnt=15";

typedef struct MemoryStruct {
	char *memory;
	size_t size;
} memoryStruct;

typedef struct appdata {
	Evas_Coord width;
	Evas_Coord height;

	Evas_Object *win;
	Evas_Object *img;

	cairo_t *cairo;
	cairo_surface_t *surface;
	cairo_surface_t *bg_surface;
	cairo_surface_t *amb_bg_surface;

	cairo_pattern_t *hour_needle_color;
	cairo_pattern_t *second_needle_color;
	cairo_pattern_t *bkg1_color;
	cairo_pattern_t *bkg2_color;

	cairo_pattern_t *amb_hour_needle_color;
	cairo_pattern_t *amb_bkg1_color;
	cairo_pattern_t *amb_bkg2_color;

	int weather_type;

	int count;
	int mouse_click;
	int mouse_press;
	int mouse_type;

	memoryStruct ms;

} appdata_s;

typedef struct timedata {
	int on;
	int alarm;
	int hour;
	int min;
	int type;

} timedata_s;

timedata_s t_data[24];

char recv_data[100];

static size_t
write_memory_cb(void *contents, size_t size, size_t nmemb, void *userp)
{
	//넣으려는 문자열의 크기에 맞춰 할당한 후, 값을 넣어줌
	size_t realsize = size * nmemb;
	memoryStruct *mem = (memoryStruct *)userp;
	mem->memory = realloc(mem->memory, mem->size + realsize + 1);
	if(mem->memory == NULL) {
		/* 할당이 안됐을 때 */
		dlog_print(DLOG_INFO, "tag", "not enough memory (realloc returned NULL)");
		return 0;
	}
	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;
	return realsize;
}

void
get_http_data(memoryStruct *data) //서버 통신 시도 함수
{
	CURL *curl_handle; 
	CURLcode res;
	data->memory = malloc(1); //memory 문자열 세팅
	data->size = 0; 
	/* curl 라이브러리 초기화 */
	curl_global_init(CURL_GLOBAL_ALL);
	/* curl 객체 생성 및 초기화 */
	curl_handle = curl_easy_init();
	/* curl 객체에 옵션을 지정하는 API  */
	curl_easy_setopt(curl_handle, CURLOPT_URL, url); //URL주소를 지정
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_memory_cb); //통신 결과 수신 콜백함수 지정
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)data); //사용자 데이터 지정
	/* 서버와 통신을 시작 (성공할 경우 CURLE_OK를 반환) */
	res = curl_easy_perform(curl_handle);
	/* curl 데이터 삭제 */
	curl_easy_cleanup(curl_handle);
	/* curl 라이브러리 전체 데이터 삭제 */
	curl_global_cleanup();
}

static void
parse_json(appdata_s *ad) //JSON 구문의 데이터 가공 함수
{
	JsonParser *parser = json_parser_new (); //JsonParser 객체를 생성
	char buf[256]; //가공한 데이터가 입력될 임시 문자열 변수
	buf[0] = '\0';
	if( json_parser_load_from_data( parser, ad->ms.memory, strlen(ad->ms.memory), NULL))
		//JsonParser 객체에 JSON 구문을 입력. 인자값( JsonParser 객체, JSON 데이터, 데이터 길이, 에러 코드 )
	{
		JsonNode *root = json_parser_get_root (parser); //JsonParser의 시작 위치를 반환
		JsonObject *obj = json_node_get_object(root); //Json 구문을 JsonObject 형식으로 반환
		
		JsonNode *temp_node = json_object_get_member ( obj,"list"); //특정 JsonObject를 반환 (두번째 인자값이 멤버 key)
		JsonArray *temp_array = json_node_get_array ( temp_node ); //Json 구문을 배열 형식으로 반환
		temp_node = json_array_get_element(temp_array, 0 ); //Json 배열의 특정 항목을 반환 (두번째 인자값이 멤버 인덱스)
		JsonObject *temp_object = json_node_get_object(temp_node);
		
		temp_node = json_object_get_member ( temp_object,"weather");
		temp_array = json_node_get_array ( temp_node );
		temp_node = json_array_get_element(temp_array, 0);
		temp_object = json_node_get_object(temp_node);
		
		sprintf(buf, "%s", json_object_get_string_member(temp_object, "main")); // JsonObject에서 데이터를 문자열 형식으로 반환 -> 이 값을 buf에 넣어줌
		/*buf에 따라 appdata의 weather_type 설정*/
		if(strcmp(buf,"Clouds")==0)
			ad->weather_type = 0;
		if(strcmp(buf,"Rain")==0)
			ad->weather_type = 1;
		if(strcmp(buf,"Snow")==0)
			ad->weather_type = 2;
		if(strcmp(buf,"Clear")==0)
			ad->weather_type = 3;

	}
}

void weather_view(appdata_s* data) //날씨 정보를 받아 해당 이미지를 설정해주는 함수
{
	appdata_s* ad = data;

	/*api에서 json 구문을 가져와 날씨 데이터를 가공*/
	get_http_data(&ad->ms);

	parse_json(ad);
	
	/*parse_json에서 설정해준 weather_type값으로 이미지 설정*/
	switch (ad->weather_type)
	{
	case 0:
		ad->bg_surface = cairo_image_surface_create_from_png("/opt/usr/apps/org.example.watch1/res/images/cloud.png");
		break;
	case 1:
		ad->bg_surface = cairo_image_surface_create_from_png("/opt/usr/apps/org.example.watch1/res/images/rainy.png");
		break;
	case 2:
		ad->bg_surface = cairo_image_surface_create_from_png("/opt/usr/apps/org.example.watch1/res/images/sunder.png");
		break;
	case 3:
		ad->bg_surface = cairo_image_surface_create_from_png("/opt/usr/apps/org.example.watch1/res/images/sunny.png");
		break;
	default:
		ad->bg_surface = cairo_image_surface_create_from_png("/opt/usr/apps/org.example.watch1/res/images/clear_bg.png");
		break;
	}

}

char *data_get_popup_text(void)
{
	char popup_text[1024] = { 0, };
	snprintf(popup_text, sizeof(popup_text), "%s", "일정이 가까이 있습니다.");

	return strdup(popup_text);
}

void view_set_text(Evas_Object *parent, const char *part_name, const char *text)
{
	if (parent == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG, "parent is NULL.");
		return;
	}
	/* Set text of target part object */
	elm_object_part_text_set(parent, part_name, text);
}

Evas_Object *view_create_text_popup(Evas_Object *parent, double timeout, const char *text)
{
	Evas_Object *popup = NULL;
	Evas_Object *popup_layout = NULL;

	if (parent == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG, "parent is NULL.");
		return NULL;
	}

	popup = elm_popup_add(parent);
	elm_object_style_set(popup, "circle");
	evas_object_size_hint_weight_set(popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

	popup_layout = elm_layout_add(popup);
	elm_layout_theme_set(popup_layout, "layout", "popup", "content/circle");

	elm_object_content_set(popup, popup_layout);

	elm_popup_timeout_set(popup, timeout);
	if (text)
		view_set_text(popup_layout, "elm.text", text);

	evas_object_show(popup);
	return popup;
}

void Create_Popup(appdata_s *data)
{
	appdata_s *ad = data;

	Evas_Object *popup = NULL;
	char *popup_text = NULL;

	popup_text = data_get_popup_text();
	popup = view_create_text_popup(ad->img, POPUP_DURATION, popup_text);
	free(popup_text);
}


bool get_AlarmData_cb(app_control_h app_control, const char *key,void *data) {
	int ret;
	int index;
	char *value;
	char *result;

	ret = app_control_get_extra_data(app_control, key, &value);
	sprintf(recv_data,"");
	strcat(recv_data,value);
	if(recv_data[0]=='H'&&recv_data[1]=='H')
	{
		result = strtok(recv_data," ");

		int i=0;
		while(result != NULL)
		{
			result = strtok(NULL," ");
			if(i==0)
			{
				index = atoi(result);

				t_data[index].hour = index;
				t_data[index].on = 1;
			}
			if(i==1)
				t_data[index].min = atoi(result);
			if(i==2)
				t_data[index].type = atoi(result);

			dlog_print(DLOG_INFO, "Oppt",result);
			i++;
		}
		t_data[index].alarm = 1;
	}
	return true;
}

static void run_application(void *user_data) {
	app_control_h app_control;
	appdata_s *ad = user_data;


	app_control_create(&app_control);
	app_control_set_operation(app_control, APP_CONTROL_OPERATION_DEFAULT);

	app_control_set_app_id(app_control, "org.example.timesettingui2");

	if (app_control_send_launch_request(app_control, NULL, NULL) ==APP_CONTROL_ERROR_NONE)
	dlog_print(DLOG_INFO, "tag", "Succeeded to launch a Helloworld app.");
	else
	dlog_print(DLOG_INFO, "tag", "Failed to launch a calculator app.");

	app_control_destroy(app_control);
}

static void mouse_down_cb(void *data, Evas *e, Evas_Object *obj,void *event_info) {

	Evas_Event_Mouse_Down *ev = event_info;
	appdata_s *ad = data;


	float x = ev->canvas.x;
	float y = ev->canvas.y;

	float distance = sqrt((x-180.0)*(x-180.0)+(y-180.0)*(y-180.0));

	if(distance<15)
		weather_view(ad);
	else
	{

		ad->mouse_click = 1;
		ad->mouse_press = 0;
		float angle = 180 - atan2((x-180.0),(y-180))*(180/M_PI);
		int angleInt = (int)(angle);
		ad->mouse_type = (angleInt/15)/2;
		if(ad->mouse_type>=12)
			ad->mouse_type = 0;

		char a[100];
		sprintf(a,"%d",ad->mouse_type);

		dlog_print(DLOG_INFO, "type", a);
	}
}
static void mouse_up_cb(void *data, Evas *e, Evas_Object *obj,void *event_info) {

	appdata_s *ad = data;
	if(ad->mouse_press<2&&ad->mouse_click==1)
	{
		run_application(ad);
		ad->mouse_press = 0;
		ad->mouse_click = 0;
		ad->mouse_type = 0;
	}
}

void calculate_time_alarm(appdata_s *data,int hour,int miniute)
{
	appdata_s *ad = data;
	int cal;
	if(t_data[hour].alarm!=0)
	{
		cal = t_data[hour].min - miniute;
		if(cal<15&&cal>=0)
		{
			t_data[hour].alarm = 0;
			Create_Popup(ad);
		}
	}
	else if(t_data[hour+1].alarm!=0)
	{
		cal = 60+t_data[hour+1].min - miniute;
		if(cal<15&&cal>=0)
		{
			t_data[hour+1].alarm = 0;
			Create_Popup(ad);
		}
	}
}

static void draw_hour_needle(cairo_t *cairo, int hours, int minutes) {
	float angle = M_PI * ((hours % 12 + minutes / 60.0) / 6);
	cairo_rotate(cairo, angle);
	cairo_rectangle(cairo, -8, 36, 16, 45 - WATCH_RADIUS);
	cairo_fill(cairo);
	cairo_rotate(cairo, -angle);
}

static void draw_minute_needle(cairo_t *cairo, int minutes, int seconds) {
	float angle = M_PI * (minutes + seconds / 60.0) / 30;
	cairo_rotate(cairo, angle);
	cairo_rectangle(cairo, -8, 36, 16, -13 - WATCH_RADIUS);
	cairo_fill(cairo);
	cairo_rotate(cairo, -angle);
}

static void draw_clock_graduation(cairo_t *cairo) {

	cairo_pattern_t *gradu_color;
	gradu_color = cairo_pattern_create_rgb(1.0, 0.720, 0.682);
	int g_width = 16;
	int g_height = 42;
	float angle;

	for (int i = 0; i < 12; i++) {
		if(t_data[i].on==1)
		{
			gradu_color = cairo_pattern_create_rgb(0.7, 0.120, 0.382);
		}
		else if(t_data[i+12].on==1)
		{
			gradu_color = cairo_pattern_create_rgb(0.7, 0.40, 0.382);
		}
		else
		{
			gradu_color = cairo_pattern_create_rgb(0.0, 0.720, 0.682);
		}

		cairo_set_source(cairo, gradu_color);
		angle = M_PI * (((i+6) % 12) / 6.0);
		cairo_rotate(cairo, angle);
		cairo_rectangle(cairo, -8, 172, g_width, -g_height);
		cairo_fill(cairo);
		cairo_rotate(cairo, -angle);
	}
}

static void drawCircle(cairo_t *cairo, int x, int y, int radius) {
	cairo_arc(cairo, x, y, radius, 0, M_PI * 2);
	cairo_fill(cairo);
}

static void draw_second_needle(cairo_t *cairo, int seconds, int milliseconds) {
	float angle = M_PI * (seconds + milliseconds / 1000.0) / 30.0;
	cairo_rotate(cairo, angle);
	cairo_rectangle(cairo, -3, 36, 6, 30 - WATCH_RADIUS);
	cairo_fill(cairo);
	drawCircle(cairo, 0, -110, 15);
	cairo_rotate(cairo, -angle);
}

static void update_watch(appdata_s *ad, watch_time_h watch_time, int ambient) {
	int hour24, minute, second, millisecond;
	cairo_pattern_t *hour_needle_color, *bkg1_color, *bkg2_color;

	if(ad->mouse_click==1)
		ad->mouse_press++;
	if(ad->mouse_press>3)
	{
		t_data[ad->mouse_type].alarm = 0;
		t_data[ad->mouse_type].on = 0;
		t_data[(ad->mouse_type+12)].alarm = 0;
		t_data[(ad->mouse_type+12)].on = 0;
		ad->mouse_press = 0;
		ad->mouse_click = 0;
		ad->mouse_type = 0;
	}

	if (ambient) {
		hour_needle_color = ad->amb_hour_needle_color;
		bkg1_color = ad->amb_bkg1_color;
		bkg2_color = ad->amb_bkg2_color;
	} else {
		hour_needle_color = ad->hour_needle_color;
		bkg1_color = ad->bkg1_color;
		bkg2_color = ad->bkg2_color;
	}

	if (watch_time == NULL)
		return;

	watch_time_get_hour24(watch_time, &hour24);
	watch_time_get_minute(watch_time, &minute);

	if (ambient) {
		cairo_set_source_surface(ad->cairo, ad->amb_bg_surface, -WATCH_RADIUS,-WATCH_RADIUS);
	} else {
		cairo_set_source_surface(ad->cairo, ad->bg_surface, -WATCH_RADIUS,-WATCH_RADIUS);
	}
	cairo_paint(ad->cairo);

	cairo_set_source(ad->cairo, hour_needle_color);
	draw_hour_needle(ad->cairo, hour24, minute);
	draw_clock_graduation(ad->cairo);
	if (ambient) {
		draw_minute_needle(ad->cairo, minute, 0);
	} else {
		watch_time_get_second(watch_time, &second);
		watch_time_get_millisecond(watch_time, &millisecond);
		cairo_set_source(ad->cairo, ad->hour_needle_color);
		draw_minute_needle(ad->cairo, minute, second);
		cairo_set_source(ad->cairo, ad->second_needle_color);
		draw_second_needle(ad->cairo, second, millisecond);
		drawCircle(ad->cairo, 0, 0, 6);
	}

	cairo_set_source(ad->cairo, bkg1_color);
	drawCircle(ad->cairo, 0, 0, 2);

	cairo_set_source(ad->cairo, bkg2_color);
	drawCircle(ad->cairo, 0, 0, 1);

	cairo_surface_flush(ad->surface);

	/* display cairo drawing on screen */
	unsigned char * imageData = cairo_image_surface_get_data(cairo_get_target(ad->cairo));
	evas_object_image_data_set(ad->img, imageData);
	evas_object_image_data_update_add(ad->img, 0, 0, ad->width, ad->height);

	calculate_time_alarm(ad,hour24,minute);
}

static void create_base_gui(appdata_s *ad, int width, int height) {
	ad->count = 0;
	ad->weather_type = 0;

	for(int i=0;i<24;i++)
	{
		t_data[i].on=0;
		t_data[i].alarm=0;
	}

	int ret;

	watch_time_h watch_time = NULL;

	/* Window */
	ret = watch_app_get_elm_win(&ad->win);
	if (ret != APP_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "failed to get window. err = %d", ret);
		return;
	}

	ad->width = width;
	ad->height = height;
	evas_object_resize(ad->win, ad->width, ad->height);

	elm_win_autodel_set(ad->win, EINA_TRUE);
	if (elm_win_wm_rotation_supported_get(ad->win)) {
		int rots[4] = { 0, 90, 180, 270 };
		elm_win_wm_rotation_available_rotations_set(ad->win,(const int *) (&rots), 4);
	}
	evas_object_show(ad->win);

	/* Image */
	ad->img = evas_object_image_filled_add(evas_object_evas_get(ad->win));
	evas_object_image_size_set(ad->img, ad->width, ad->height);
	evas_object_resize(ad->img, ad->width, ad->height);
	evas_object_show(ad->img);
	evas_object_event_callback_add(ad->img, EVAS_CALLBACK_MOUSE_DOWN,mouse_down_cb, ad);
	evas_object_event_callback_add(ad->img, EVAS_CALLBACK_MOUSE_UP,mouse_up_cb, ad);

	ad->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ad->width,
			ad->height);
	ad->cairo = cairo_create(ad->surface);

	/* draw watch face background */
	ad->bg_surface = cairo_image_surface_create_from_png("/opt/usr/apps/org.example.watch1/res/images/clear_bg.png");
	ad->amb_bg_surface = cairo_image_surface_create_from_png(
			"/opt/usr/apps/org.example.watch1/res/images/dark_bg.png");
	if (ad->bg_surface == NULL || ad->amb_bg_surface == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG,"Could not create background image from file");
	}

	ad->hour_needle_color = cairo_pattern_create_rgb(0.19, 0.19, 0.19);
	ad->second_needle_color = cairo_pattern_create_rgb(0.745, 0.0, 0.062);
	ad->bkg1_color = cairo_pattern_create_rgb(0.733, 0.733, 0.733);
	ad->bkg2_color = cairo_pattern_create_rgb(0.0, 0.0, 0.0);

	ad->amb_hour_needle_color = cairo_pattern_create_rgb(0.86, 0.86, 0.86);
	ad->amb_bkg1_color = cairo_pattern_create_rgb(0.733, 0.733, 0.733);
	ad->amb_bkg2_color = cairo_pattern_create_rgb(0.0, 0.0, 0.0);

	cairo_translate(ad->cairo, WATCH_RADIUS, WATCH_RADIUS);

	ret = watch_time_get_current_time(&watch_time);
	if (ret != APP_ERROR_NONE)
		dlog_print(DLOG_ERROR, LOG_TAG, "failed to get current time. err = %d",
				ret);

	update_watch(ad, watch_time, 0);

	watch_time_delete(watch_time);
}

static bool app_create(int width, int height, void *data) {
	/* Hook to take necessary actions before main event loop starts
	 Initialize UI resources and application's data
	 If this function returns true, the main loop of application starts
	 If this function returns false, the application is terminated */
	appdata_s *ad = data;

	create_base_gui(ad, width, height);

	return true;
}

static void app_control(app_control_h app_control, void *data) {
	/* Handle the launch request. */
	app_control_foreach_extra_data(app_control, get_AlarmData_cb,data);

}

static void app_pause(void *data) {
	/* Take necessary actions when application becomes invisible. */
}

static void app_resume(void *data) {
	/* Take necessary actions when application becomes visible. */
}

static void app_terminate(void *data) {
	/* Release all resources. */
}

static void app_time_tick(watch_time_h watch_time, void *data) {
	/* Called at each second while your app is visible. Update watch UI. */
	appdata_s *ad = data;
	update_watch(ad, watch_time, 0);
}

static void app_ambient_tick(watch_time_h watch_time, void *data) {
	/* Called at each minute while the device is in ambient mode. Update watch UI. */
	appdata_s *ad = data;
	update_watch(ad, watch_time, 1);
}

static void app_ambient_changed(bool ambient_mode, void *data) {
	/* Update your watch UI to conform to the ambient mode */
}

static void watch_app_lang_changed(app_event_info_h event_info, void *user_data) {
	/*APP_EVENT_LANGUAGE_CHANGED*/
	char *locale = NULL;
	app_event_get_language(event_info, &locale);
	elm_language_set(locale);
	free(locale);
	return;
}

static void watch_app_region_changed(app_event_info_h event_info,
		void *user_data) {
	/*APP_EVENT_REGION_FORMAT_CHANGED*/
}

int main(int argc, char *argv[]) {
	appdata_s ad = { 0, };
	int ret = 0;

	watch_app_lifecycle_callback_s event_callback = { 0, };
	app_event_handler_h handlers[5] = { NULL, };

	event_callback.create = app_create;
	event_callback.terminate = app_terminate;
	event_callback.pause = app_pause;
	event_callback.resume = app_resume;
	event_callback.app_control = app_control;
	event_callback.time_tick = app_time_tick;
	event_callback.ambient_tick = app_ambient_tick;
	event_callback.ambient_changed = app_ambient_changed;

	watch_app_add_event_handler(&handlers[APP_EVENT_LANGUAGE_CHANGED],
			APP_EVENT_LANGUAGE_CHANGED, watch_app_lang_changed, &ad);
	watch_app_add_event_handler(&handlers[APP_EVENT_REGION_FORMAT_CHANGED],
			APP_EVENT_REGION_FORMAT_CHANGED, watch_app_region_changed, &ad);

	ret = watch_app_main(argc, argv, &event_callback, &ad);
	if (ret != APP_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "watch_app_main() is failed. err = %d",
				ret);
	}

	return ret;
}
