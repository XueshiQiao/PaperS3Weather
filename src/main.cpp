#include "Logger.h"
#include <M5Unified.h>
#include <WiFi.h>
#include <Preferences.h>

#include "constants.h"
#include "utils.h"
#include "weather.h"
#include "config.h"
#include "display.h"

Preferences preferences;
M5GFX& canvas = M5.Display;
WeatherData currentWeather;

bool useCelsius = false;
bool nightModeSleep = true;
String cityName = DEFAULT_CITY;

String qweather_proxy_host_token = "92508b78-f4a5-46c2-958a-557f18b6a2f9";
String qweather_proxy_host = "http://192.168.8.8:3000";
String qweather_location = "101010600";
String qweather_response_lang = "en";

WeatherManager* weatherManager = nullptr;

unsigned long lastRefreshTime = 0;
int refreshCounter = 0;

int qweatherIconToWMO(const String& iconCode);

void mapQWeatherToWeatherData() {
    if (!weatherManager) return;

    auto qweather = weatherManager->getCurrentWeather();
    if (!qweather) return;

    currentWeather.temperature = qweather->temp;
    currentWeather.apparentTemperature = qweather->feelsLike;
    currentWeather.humidity = qweather->humidity;
    currentWeather.windSpeed = qweather->windSpeed;
    currentWeather.windDir = qweather->wind360;
    currentWeather.precipitation = qweather->precip;

    String iconCode = qweather->icon;
    currentWeather.weatherCode = qweatherIconToWMO(iconCode);

    auto hourly = weatherManager->getHourlyWeathers();
    if (hourly) {
        int count = min((int)hourly->size(), MAX_HOURLY);
        for (int i = 0; i < count; i++) {
            currentWeather.hourly[i].temp = (*hourly)[i].temp;
            currentWeather.hourly[i].humidity = (*hourly)[i].humidity;
            currentWeather.hourly[i].precip = (*hourly)[i].precip;
            currentWeather.hourly[i].pressure = (*hourly)[i].pressure;
            currentWeather.hourly[i].uvIndex = ((*hourly)[i].pop > 0) ? (*hourly)[i].pop / 100.0f : 0.0f;
            currentWeather.hourly[i].weatherCode = qweatherIconToWMO((*hourly)[i].icon);
        }
    }

    auto daily = weatherManager->getDailyWeathers();
    if (daily && daily->size() > 0) {
        currentWeather.todayMinTemp = (*daily)[0].tempMin;
        currentWeather.todayMaxTemp = (*daily)[0].tempMax;

        int count = min((int)daily->size(), MAX_FORECAST);
        for (int i = 0; i < count; i++) {
            currentWeather.forecastMaxTemp[i] = (*daily)[i].tempMax;
            currentWeather.forecastMinTemp[i] = (*daily)[i].tempMin;
            currentWeather.forecastRain[i] = (*daily)[i].precip;
            currentWeather.forecastHumidity[i] = (*daily)[i].humidity;
            currentWeather.forecastPressure[i] = (*daily)[i].pressure;
        }

        if ((*daily)[0].sunrise.length() >= 5) {
            currentWeather.sunriseTime = (*daily)[0].sunrise.substring(11, 16);
        }
        if ((*daily)[0].sunset.length() >= 5) {
            currentWeather.sunsetTime = (*daily)[0].sunset.substring(11, 16);
        }
    }
}

int qweatherIconToWMO(const String& iconCode) {
    int code = iconCode.toInt();
    if (code <= 0) return 0;

    if (code >= 100 && code <= 104) return 0;
    if (code >= 150 && code <= 154) return 1;
    if (code >= 200 && code <= 213) return 2;
    if (code >= 300 && code <= 320) return 3;
    if (code >= 400 && code <= 499) return 51;
    if (code >= 500 && code <= 599) return 61;
    if (code >= 600 && code <= 699) return 71;
    if (code >= 700 && code <= 999) return 95;

    return 0;
}

void refreshWeather() {
    my_log("--- Starting Weather Refresh ---");

    if (WiFi.status() != WL_CONNECTED) {
        my_log("WiFi not connected, attempting to reconnect...");
        setupWiFi();
    }

    if (WiFi.status() == WL_CONNECTED) {
        my_log("WiFi Connected. Syncing time...");
        configTime(TIMEZONE_OFFSET_HOURS * 3600, 0, NTP_SERVER_1, NTP_SERVER_2);

        float latitude, longitude;
        loadPreferences(latitude, longitude, cityName);
        qweather_location = String(latitude, 4) + "," + String(longitude, 4);

        my_log_f("Fetching weather for: %s (%s)", cityName.c_str(), qweather_location.c_str());

        bool fetchSuccess = false;
        for (int retry = 0; retry < HTTP_RETRY_ATTEMPTS; retry++) {
            if (retry > 0) {
                my_log_f("Weather fetch retry %d/%d...", retry + 1, HTTP_RETRY_ATTEMPTS);
                delay(HTTP_RETRY_DELAY_MS);
            }

            if (weatherManager->requestWeatherNow() &&
                weatherManager->requestHourlyForecasts(24) &&
                weatherManager->requestDailyForecasts(7)) {
                mapQWeatherToWeatherData();

                my_log_f("QWeather mapped - Temp: %.1f, Humidity: %.0f%%",
                         currentWeather.temperature, currentWeather.humidity);

                my_log("Weather fetch successful! Drawing to display...");
                displayWeather();
                my_log("Display update command sent.");
                lastRefreshTime = millis();
                fetchSuccess = true;
                break;
            } else {
                my_log("Weather fetch failed.");
            }
        }

        if (!fetchSuccess) {
            my_log("All weather fetch attempts failed!");
        }
    } else {
        my_log("WiFi failed to connect.");
    }
    my_log("--- Refresh Complete ---");
}

void enterLightSleep(unsigned long sleepTimeMs) {
    my_log_f("Preparing for LIGHT sleep for %lu ms (%lu minutes)",
                  sleepTimeMs, sleepTimeMs / 60000);

    if (M5.Display.displayBusy()) {
        my_log("Waiting for display to finish...");
        M5.Display.waitDisplay();
    }
    my_log("complete display");

    esp_sleep_enable_timer_wakeup(sleepTimeMs * 1000);

    Serial.flush();

    esp_light_sleep_start();

    my_log("Woke up from Light Sleep!");
}

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);

    delay(100);

    my_log("=================================");
    my_log("PaperS3Weather " + String(VERSION));
    my_log("System Starting (Light Sleep Mode) - QWeather Edition...");

    M5.Display.startWrite();
    M5.Display.fillScreen(TFT_WHITE);
    M5.Display.endWrite();
    M5.Display.display();
    my_log("Display cleared");

    M5.Display.setRotation(1);

    M5.Display.startWrite();
    M5.Display.fillScreen(TFT_WHITE);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(20, 20);
    M5.Display.println("PaperS3Weather " + String(VERSION));
    M5.Display.println("Initializing QWeather...");
    M5.Display.endWrite();
    M5.Display.display();
    delay(1000);

    setupWiFi();

    // preferences.begin("weather", true);
    // qweather_api_key = preferences.getString("qweather_key", "");
    // qweather_host = preferences.getString("qweather_host", "n97mda5jxn.re.qweatherapi.com");
    // preferences.end();

    if (qweather_proxy_host_token.length() == 0 || qweather_proxy_host.length() == 0) {
        my_log("WARNING: QWeather Host or Token not found in preferences!");
        my_log("Please configure in config portal.");
    } else {
        my_log_f("QWeather Host configured: %s...", qweather_proxy_host.substring(0, 10).c_str());
        my_log_f("QWeather Host key configured: %s...", qweather_proxy_host_token.substring(0, 10).c_str());
    }

    if (weatherManager == nullptr) {
        weatherManager = new WeatherManager(qweather_location, qweather_proxy_host_token, qweather_proxy_host);
        weatherManager->setMetricUnit(true);
    }

    preferences.begin("weather", false);
    if (preferences.getInt("day_interval", 1) == 10) {
        my_log("Migrating refresh interval from 10 to 1 min...");
        preferences.putInt("day_interval", 1);
    }
    preferences.end();

    my_log("Setup complete!");
}

bool checkTouchInTimeArea() {
    auto touch = M5.Touch.getDetail();
    if (!touch.isPressed()) {
        return false;
    }

    int x = touch.x;
    int y = touch.y;

    int timePanelX = PANEL_SPACING;
    int timePanelY = PANEL_TITLE_HEIGHT;
    int timePanelWidth = (SCREEN_WIDTH - 30) / 2;
    int timePanelHeight = 251;

    if (x >= timePanelX && x <= timePanelX + timePanelWidth &&
        y >= timePanelY && y <= timePanelY + timePanelHeight) {
        my_log_f("Touch detected in time area at (%d, %d)", x, y);
        return true;
    }

    return false;
}

void loop() {
    refreshWeather();

    unsigned long sleepTime = getRefreshInterval();

    preferences.begin("weather", true);
    int nightStart = preferences.getInt("night_start", 22);
    int nightEnd = preferences.getInt("night_end", 5);
    preferences.end();

    my_log("=================================");
    my_log_f("Night mode: %s", nightModeSleep ? "ENABLED" : "DISABLED");
    if (nightModeSleep) {
        my_log_f("Night hours: %d:00 - %d:00", nightStart, nightEnd);
    }
    my_log_f("Current time is: %s", isNightTime() ? "NIGHT" : "DAY");
    my_log_f("Refresh interval: %lu minutes", sleepTime / 60000);
    my_log("=================================");

    my_log("Waiting for touch input...");
    unsigned long touchStart = millis();
    while (millis() - touchStart < 5000) {
        if (checkTouchInTimeArea()) {
            my_log("Touch in time area - refreshing time display only...");
            displayWeather();
            my_log("Time display refreshed!");
            touchStart = millis();
        }
        delay(100);
    }
    my_log("No touch detected, entering sleep mode...");

    enterLightSleep(sleepTime);
}
