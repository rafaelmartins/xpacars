#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <XPLM/XPLMDataAccess.h>
#include <XPLM/XPLMMenus.h>
#include <XPLM/XPLMProcessing.h>
#include <XPLM/XPLMUtilities.h>

typedef struct {
    char aircraft_icao[40];
    char aircraft_tailnum[40];
    char aircraft_description[260];
    // want to add origin and destination here too, but need to figure a simple
    // way to ask users to input it.
} flight_data_t;

typedef struct {
    double latitude;
    double longitude;
    double altitude;
    float track;
    float ground_speed;
    float air_speed;
    float vertical_speed;
} position_data_t;

typedef struct {
    char *str;
    size_t size;
} str_buf_t;

static XPLMDataRef *aircraft_icao_dr = NULL;
static XPLMDataRef *aircraft_tailnum_dr = NULL;
static XPLMDataRef *aircraft_description_dr = NULL;
static XPLMDataRef *latitude_dr = NULL;
static XPLMDataRef *longitude_dr = NULL;
static XPLMDataRef *altitude_dr = NULL;
static XPLMDataRef *track_dr = NULL;
static XPLMDataRef *ground_speed_dr = NULL;
static XPLMDataRef *air_speed_dr = NULL;
static XPLMDataRef *vertical_speed_dr = NULL;

static char url[1024];
static bool sending = false;
static long long int flight_id = -1;

static int main_submenu;
static XPLMMenuID status_menu;
static int status_index;


/**
 * xpacars protocol, version 0
 *
 * This is the definition of our protocol. All the fields are separated with
 * a newline '\n' character.
 *
 * We have 2 types of requests, that are defined by the Content-Type header.
 */

/**
 * Content-Type: application/vnd.xpacars.flight
 *
 * This request registers a new flight in the server. It just contains the
 * aircraft data for now, but information about origin airport and destination
 * aiport should be added in a new version of the protocol.
 *
 * The server should return 201 to notify that the new flight was registered,
 * and the ID of the flight, that will be used by the upcoming requests.
 */
const char *flight_format =
    "1\n"   // protocol version
    "%s\n"  // aircraft icao
    "%s\n"  // aircraft tailnum
    "%s\n"  // aircraft description
    "";

/**
 * Content-Type: application/vnd.xpacars.position
 *
 * This request send information about the current status of the aircraft to
 * the server.
 *
 * The server should return 202 to notify that accepted the data. When a
 * request is not accepted, the client may take some action to recover, like
 * stop sending for a while, or increase the send intervals.
 *
 * There's no guarantee that the data was processed by the server right after
 * the request. The server can do the processing asyncronously.
 */
const char *position_format =
    "1\n"     // protocol version
    "%lld\n"  // flight id
    "%f\n"    // latitude in degrees
    "%f\n"    // longitude in degrees
    "%f\n"    // altitude in meters (not feets!)
    "%f\n"    // track in degrees
    "%f\n"    // ground speed in meters per second (not knots!)
    "%f\n"    // air speed in meters per second (not knots!)
    "%f\n"    // vertical speed meters per second (not feets per minute!)
    "";


static size_t
SendFlightDataCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t realsize = size * nmemb;
    str_buf_t *buf = userdata;

    buf->str = realloc(buf->str, buf->size + realsize + 1);
    if (buf->str == NULL) {
        printf("error: failed to realloc memory\n");
        return 0;
    }

    memcpy(&(buf->str[buf->size]), ptr, realsize);
    buf->size += realsize;
    buf->str[buf->size] = '\0';

    return realsize;
}


static bool
SendFlightData(const char *url, flight_data_t *data)
{
    if (url == NULL || data == NULL)
        return false;

    int body_len = snprintf(NULL, 0, flight_format, data->aircraft_icao,
        data->aircraft_tailnum, data->aircraft_description);

    if (body_len <= 0)
        return false;

    char *body = malloc(body_len + 1);
    if (body == NULL)
        return false;

    snprintf(body, body_len + 1, flight_format, data->aircraft_icao,
        data->aircraft_tailnum, data->aircraft_description);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers,
        "Content-Type: application/vnd.xpacars.flight");

    str_buf_t buf;
    buf.str = malloc(1);
    buf.size = 0;

    CURL *hnd = curl_easy_init();
    curl_easy_setopt(hnd, CURLOPT_URL, url);
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, SendFlightDataCallback);
    curl_easy_setopt(hnd, CURLOPT_WRITEDATA, (void *) &buf);
    curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(hnd, CURLOPT_USERAGENT, PACKAGE_NAME "/" PACKAGE_VERSION);
    curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, body);

    bool rv = curl_easy_perform(hnd) == CURLE_OK;
    if (rv) {
        long http_code = 0;
        curl_easy_getinfo(hnd, CURLINFO_RESPONSE_CODE, &http_code);
        rv = http_code == 201;
        if (rv) {
            long long int tmp_id = strtoll(buf.str, NULL, 10);
            if (tmp_id >= 1)
                flight_id = tmp_id;
        }
    }

    free(buf.str);
    free(body);
    curl_slist_free_all(headers);
    curl_easy_cleanup(hnd);

    return rv;
}


static bool
SendPositionData(const char *url, position_data_t *data)
{
    if (url == NULL || data == NULL)
        return false;

    int body_len = snprintf(NULL, 0, position_format, flight_id, data->latitude,
        data->longitude, data->altitude, data->track, data->ground_speed,
        data->air_speed, data->vertical_speed);

    if (body_len <= 0)
        return false;

    char *body = malloc(body_len + 1);
    if (body == NULL)
        return false;

    snprintf(body, body_len + 1, position_format, flight_id, data->latitude,
        data->longitude, data->altitude, data->track, data->ground_speed,
        data->air_speed, data->vertical_speed);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers,
        "Content-Type: application/vnd.xpacars.position");

    CURL *hnd = curl_easy_init();
    curl_easy_setopt(hnd, CURLOPT_URL, url);
    curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(hnd, CURLOPT_USERAGENT, PACKAGE_NAME "/" PACKAGE_VERSION);
    curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, body);

    bool rv = curl_easy_perform(hnd) == CURLE_OK;
    if (rv) {
        long http_code = 0;
        curl_easy_getinfo(hnd, CURLINFO_RESPONSE_CODE, &http_code);
        rv = http_code == 202;
    }

    free(body);
    curl_slist_free_all(headers);
    curl_easy_cleanup(hnd);

    return rv;
}


static char*
str_lstrip(char *str)
{
    if (str == NULL)
        return NULL;
    int i;
    size_t str_len = strlen(str);
    for (i = 0; i < str_len; i++) {
        if ((str[i] != ' ') && (str[i] != '\t') && (str[i] != '\n') &&
            (str[i] != '\r') && (str[i] != '\t') && (str[i] != '\f') &&
            (str[i] != '\v'))
        {
            str += i;
            break;
        }
        if (i == str_len - 1) {
            str += str_len;
            break;
        }
    }
    return str;
}


static char*
str_rstrip(char *str)
{
    if (str == NULL)
        return NULL;
    int i;
    size_t str_len = strlen(str);
    for (i = str_len - 1; i >= 0; i--) {
        if ((str[i] != ' ') && (str[i] != '\t') && (str[i] != '\n') &&
            (str[i] != '\r') && (str[i] != '\t') && (str[i] != '\f') &&
            (str[i] != '\v'))
        {
            str[i + 1] = '\0';
            break;
        }
        if (i == 0) {
            str[0] = '\0';
            break;
        }
    }
    return str;
}


char*
str_strip(char *str)
{
    return str_lstrip(str_rstrip(str));
}


static float
FlightLoopCallback(float elapsedMe, float elapsedSim, int counter, void *refcon)
{
    // FIXME: make this global, and increment with retries
    float time = 15;  // in seconds

    if (flight_id == -1) {
        flight_data_t flight_data;
        XPLMGetDatab(aircraft_icao_dr, flight_data.aircraft_icao, 0, 40);
        if (flight_data.aircraft_icao == NULL)
            return time;

        XPLMGetDatab(aircraft_tailnum_dr, flight_data.aircraft_tailnum, 0, 40);
        if (flight_data.aircraft_tailnum == NULL)
            return time;

        XPLMGetDatab(aircraft_description_dr, flight_data.aircraft_description,
            0, 260);
        if (flight_data.aircraft_description == NULL)
            return time;

        if (!SendFlightData(str_strip(url), &flight_data) || flight_id == -1) {
            XPLMSetMenuItemName(status_menu, status_index,
                "Status: Initialization failed", 1);
            return time;
        }
        sending = false;
    }

    position_data_t data;
    data.latitude = XPLMGetDatad(latitude_dr);
    data.longitude = XPLMGetDatad(longitude_dr);
    data.altitude = XPLMGetDatad(altitude_dr);
    data.track = XPLMGetDataf(track_dr);
    data.ground_speed = XPLMGetDataf(ground_speed_dr);
    data.air_speed = XPLMGetDataf(air_speed_dr);
    data.vertical_speed = XPLMGetDataf(vertical_speed_dr);

    if (!SendPositionData(str_strip(url), &data)) {
        if (sending) {
            XPLMSetMenuItemName(status_menu, status_index, "Status: Failed", 1);
            sending = false;
            return time;
        }
    }
    else if (!sending) {
        XPLMSetMenuItemName(status_menu, status_index, "Status: Ok", 1);
        sending = true;
    }

    return 3;
}


PLUGIN_API int
XPluginStart(char *outName, char * outSig, char *outDesc)
{
    strcpy(outName, PACKAGE_STRING);
    strcpy(outSig, "io.rgm.xpacars");
    strcpy(outDesc, "A plugin that sends position data to a remote HTTP endpoint");

    aircraft_icao_dr = XPLMFindDataRef("sim/aircraft/view/acf_ICAO");
    if (aircraft_icao_dr == NULL)
        return 0;

    aircraft_tailnum_dr = XPLMFindDataRef("sim/aircraft/view/acf_tailnum");
    if (aircraft_tailnum_dr == NULL)
        return 0;

    aircraft_description_dr = XPLMFindDataRef("sim/aircraft/view/acf_descrip");
    if (aircraft_description_dr == NULL)
        return 0;

    latitude_dr = XPLMFindDataRef("sim/flightmodel/position/latitude");
    if (latitude_dr == NULL)
        return 0;

    longitude_dr = XPLMFindDataRef("sim/flightmodel/position/longitude");
    if (longitude_dr == NULL)
        return 0;

    altitude_dr = XPLMFindDataRef("sim/flightmodel/position/elevation");
    if (altitude_dr == NULL)
        return 0;

    track_dr = XPLMFindDataRef("sim/flightmodel/position/mag_psi");
    if (track_dr == NULL)
        return 0;

    ground_speed_dr = XPLMFindDataRef("sim/flightmodel/position/groundspeed");
    if (ground_speed_dr == NULL)
        return 0;

    air_speed_dr = XPLMFindDataRef("sim/flightmodel/position/true_airspeed");
    if (air_speed_dr == NULL)
        return 0;

    vertical_speed_dr = XPLMFindDataRef("sim/flightmodel/position/vh_ind");
    if (vertical_speed_dr == NULL)
        return 0;

    return 1;
}


PLUGIN_API void
XPluginStop(void)
{
}


PLUGIN_API void
XPluginDisable(void)
{
    XPLMUnregisterFlightLoopCallback(FlightLoopCallback, NULL);
    XPLMClearAllMenuItems(status_menu);
    XPLMDestroyMenu(status_menu);
    XPLMRemoveMenuItem(XPLMFindPluginsMenu(), main_submenu);
    flight_id = -1;
    sending = false;
}


PLUGIN_API int
XPluginEnable(void)
{
    char config_file[1024];
    XPLMGetSystemPath(config_file);
    strcat(config_file, "Resources");
    strcat(config_file, XPLMGetDirectorySeparator());
    strcat(config_file, "plugins");
    strcat(config_file, XPLMGetDirectorySeparator());
    strcat(config_file, "xpacars.txt");

    FILE *fp = fopen(config_file, "r");
    if (fp == NULL)
        return 0;

    size_t read_len = fread(url, sizeof(char), 1024, fp);
    if (!(read_len > 0 && feof(fp))) {
        fclose(fp);
        return 0;
    }
    fclose(fp);

    if (url == NULL)
        return 0;

    int main_submenu = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "xpacars", 0, 1);
    status_menu = XPLMCreateMenu("xpacars", XPLMFindPluginsMenu(),
        main_submenu, NULL, 0);

    status_index = XPLMAppendMenuItem(status_menu, "Status: Initializing",
        NULL, 1);
    XPLMEnableMenuItem(status_menu, status_index, 0);

    XPLMRegisterFlightLoopCallback(FlightLoopCallback, -1, 0);

    return 1;
}


PLUGIN_API void
XPluginReceiveMessage(XPLMPluginID inFromWho, int inMessage, void *inParam)
{
}
