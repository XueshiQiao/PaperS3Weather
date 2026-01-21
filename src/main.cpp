#include "Logger.h"
#include <M5Unified.h>
#include <WiFi.h>
#include <Preferences.h>

// Include all our new modular headers
#include "constants.h"
#include "utils.h"
#include "weather_api.h"
#include "config.h"
#include "display.h"

// Global objects
Preferences preferences;
M5GFX& canvas = M5.Display; // Use M5.Display directly as 'canvas'
WeatherData currentWeather;

// Configuration state
bool useCelsius = false;
bool nightModeSleep = true;
String cityName = DEFAULT_CITY;

// Runtime state
unsigned long lastRefreshTime = 0;
int refreshCounter = 0;

// Helper to manage the update process
void refreshWeather() {
    my_log("--- Starting Weather Refresh ---");

    if (WiFi.status() != WL_CONNECTED) {
        my_log("WiFi not connected, attempting to reconnect...");
        setupWiFi(); // Re-run connection logic
    }

    if (WiFi.status() == WL_CONNECTED) {
        my_log("WiFi Connected. Syncing time...");
        // Sync time if needed (important after sleep)
        configTime(TIMEZONE_OFFSET_HOURS * 3600, 0, NTP_SERVER_1, NTP_SERVER_2);

        float latitude, longitude;
        loadPreferences(latitude, longitude, cityName);

        my_log_f("Fetching weather for: %.4f, %.4f (%s)",
                     latitude, longitude, cityName.c_str());

        bool fetchSuccess = false;
        for (int retry = 0; retry < HTTP_RETRY_ATTEMPTS; retry++) {
            if (retry > 0) {
                my_log_f("Weather fetch retry %d/%d...", retry + 1, HTTP_RETRY_ATTEMPTS);
                delay(HTTP_RETRY_DELAY_MS);
            }

            if (fetchWeatherData(latitude, longitude)) {
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

    // Ensure display operations are finished before sleeping
    if (M5.Display.displayBusy()) {
        my_log("Waiting for display to finish...");
        M5.Display.waitDisplay();
    }
    my_log("complete display");

    // 1. Configure wakeup source
    esp_sleep_enable_timer_wakeup(sleepTimeMs * 1000);

    Serial.flush();

    // 2. Enter Light Sleep
    esp_light_sleep_start();

    my_log("Woke up from Light Sleep!");
}
void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);

    // Small delay to ensure display is fully initialized after wake
    delay(100);

    my_log("=================================");
    my_log("PaperS3Weather " + String(VERSION));
    my_log("System Starting (Light Sleep Mode)...");

    // Force clear display immediately to prove we have control
    M5.Display.startWrite();
    M5.Display.fillScreen(TFT_WHITE);
    M5.Display.endWrite();
    M5.Display.display();
    my_log("Display cleared");

    // Configure display
    M5.Display.setRotation(1);

    // Show splash
    M5.Display.startWrite();
    M5.Display.fillScreen(TFT_WHITE);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(20, 20);
    M5.Display.println("PaperS3Weather " + String(VERSION));
    M5.Display.println("Initializing...");
    M5.Display.endWrite();
    M5.Display.display();
    delay(1000);

    // Initial Connection
    setupWiFi();

    // Initial Migration Check
    preferences.begin("weather", false);
    if (preferences.getInt("day_interval", 1) == 10) {
        my_log("Migrating refresh interval from 10 to 1 min...");
        preferences.putInt("day_interval", 1);
    }
    preferences.end();

    my_log("Setup complete!");
}

void loop() {
    // 1. Refresh Data
    refreshWeather();

    // 2. Calculate Sleep Duration
    unsigned long sleepTime = getRefreshInterval();

    // Get current settings for debug
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

    // 3. Sleep
    enterLightSleep(sleepTime);

    // Loop repeats immediately after wake...
}
