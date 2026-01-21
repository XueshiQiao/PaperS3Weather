#include "weather_api.h"
#include "constants.h"
#include "Logger.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

extern bool useCelsius;
extern WeatherData currentWeather;

bool fetchWeatherData(float latitude, float longitude) {
    if (WiFi.status() != WL_CONNECTED) {
        my_log("WiFi not connected");
        return false;
    }

    HTTPClient http;
    String url = "https://api.open-meteo.com/v1/forecast?";
    url += "latitude=" + String(latitude, 4);
    url += "&longitude=" + String(longitude, 4);
    url += "&current=temperature_2m,apparent_temperature,relative_humidity_2m,precipitation,wind_speed_10m,wind_direction_10m,weather_code";
    url += "&hourly=temperature_2m,precipitation_probability,relative_humidity_2m,pressure_msl,uv_index,weather_code";
    url += "&daily=temperature_2m_max,temperature_2m_min,precipitation_sum,relative_humidity_2m_mean,pressure_msl_mean,sunrise,sunset";
    url += useCelsius ? "&temperature_unit=celsius&wind_speed_unit=kmh&precipitation_unit=mm" :
                        "&temperature_unit=fahrenheit&wind_speed_unit=mph&precipitation_unit=inch";
    url += "&timezone=auto&forecast_days=7";

    for (int retry = 0; retry < HTTP_RETRY_ATTEMPTS; retry++) {
        if (retry > 0) {
            my_log_f("Retry attempt %d/%d...", retry + 1, HTTP_RETRY_ATTEMPTS);
            delay(HTTP_RETRY_DELAY_MS);
        }

        http.begin(url);
        http.setTimeout(HTTP_TIMEOUT_MS);

        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();

            // Parse JSON with properly sized document
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (!error) {
                // Extract current conditions
                currentWeather.temperature = doc["current"]["temperature_2m"];
                currentWeather.apparentTemperature = doc["current"]["apparent_temperature"];
                currentWeather.humidity = doc["current"]["relative_humidity_2m"];
                currentWeather.windSpeed = doc["current"]["wind_speed_10m"];
                currentWeather.windDir = doc["current"]["wind_direction_10m"];
                currentWeather.precipitation = doc["current"]["precipitation"];
                currentWeather.weatherCode = doc["current"]["weather_code"];

                my_log("=== Current Conditions from API ===");
                my_log_f("Temperature: %.1f", currentWeather.temperature);
                my_log_f("Feels Like: %.1f", currentWeather.apparentTemperature);
                my_log_f("Humidity: %.0f%%", currentWeather.humidity);
                my_log_f("Wind: %.1f @ %.0f°", currentWeather.windSpeed, currentWeather.windDir);
                my_log_f("Weather Code: %d", currentWeather.weatherCode);

                // Extract sunrise/sunset
                String sunriseStr = doc["daily"]["sunrise"][0].as<String>();
                String sunsetStr = doc["daily"]["sunset"][0].as<String>();
                if (sunriseStr.length() > 10) {
                    currentWeather.sunriseTime = sunriseStr.substring(11, 16);
                }
                if (sunsetStr.length() > 10) {
                    currentWeather.sunsetTime = sunsetStr.substring(11, 16);
                }

                // Get current hour for indexing
                struct tm timeinfo;
                if (!getLocalTime(&timeinfo)) {
                    timeinfo.tm_hour = 0;
                }
                int currentHourOffset = timeinfo.tm_hour;

                // Extract hourly data
                JsonArray hourlyTemp = doc["hourly"]["temperature_2m"];
                JsonArray hourlyPrecip = doc["hourly"]["precipitation_probability"];
                JsonArray hourlyHumidity = doc["hourly"]["relative_humidity_2m"];
                JsonArray hourlyPressure = doc["hourly"]["pressure_msl"];
                JsonArray hourlyUV = doc["hourly"]["uv_index"];
                JsonArray hourlyWeatherCode = doc["hourly"]["weather_code"];

                my_log_f("Current hour: %d, using as offset into API arrays", currentHourOffset);

                for (int i = 0; i < MAX_HOURLY && (currentHourOffset + i) < hourlyTemp.size(); i++) {
                    int apiIndex = currentHourOffset + i;
                    currentWeather.hourly[i].temp = hourlyTemp[apiIndex];
                    currentWeather.hourly[i].precip = hourlyPrecip[apiIndex];
                    currentWeather.hourly[i].humidity = hourlyHumidity[apiIndex];
                    currentWeather.hourly[i].pressure = hourlyPressure[apiIndex];
                    currentWeather.hourly[i].uvIndex = (apiIndex < hourlyUV.size()) ? hourlyUV[apiIndex].as<float>() : 0.0f;
                    currentWeather.hourly[i].weatherCode = hourlyWeatherCode[apiIndex];
                    my_log_f("Hour %d (API index %d) UV: %.1f", i, apiIndex, currentWeather.hourly[i].uvIndex);
                }

                // Extract daily forecast
                JsonArray dailyMax = doc["daily"]["temperature_2m_max"];
                JsonArray dailyMin = doc["daily"]["temperature_2m_min"];
                JsonArray dailyRain = doc["daily"]["precipitation_sum"];
                JsonArray dailyHumid = doc["daily"]["relative_humidity_2m_mean"];
                JsonArray dailyPressure = doc["daily"]["pressure_msl_mean"];

                if (dailyMax.size() > 0 && dailyMin.size() > 0) {
                    currentWeather.todayMinTemp = dailyMin[0];
                    currentWeather.todayMaxTemp = dailyMax[0];
                }

                for (int i = 0; i < MAX_FORECAST && i < dailyMax.size(); i++) {
                    currentWeather.forecastMaxTemp[i] = dailyMax[i];
                    currentWeather.forecastMinTemp[i] = dailyMin[i];
                    currentWeather.forecastRain[i] = dailyRain[i];
                    currentWeather.forecastHumidity[i] = dailyHumid[i];
                    if (i < dailyPressure.size()) {
                        currentWeather.forecastPressure[i] = dailyPressure[i];
                    }
                }

                http.end();
                my_log("Weather fetch successful!");
                return true;
            } else {
                my_log_f("JSON parsing error: %s", error.c_str());
            }
        } else {
            my_log_f("HTTP error code: %d", httpCode);
        }

        http.end();

        // If this was the last retry, break
        if (retry == HTTP_RETRY_ATTEMPTS - 1) {
            my_log("All retry attempts failed");
        }
    }

    return false;
}

bool geocodeCity(String cityName, float &latitude, float &longitude) {
    if (WiFi.status() != WL_CONNECTED) {
        my_log("WiFi not connected");
        return false;
    }

    HTTPClient http;
    String url = "https://geocoding-api.open-meteo.com/v1/search?name=";
    url += urlEncode(cityName);
    url += "&count=1&language=en&format=json";

    my_log("Geocoding city: " + cityName);

    for (int retry = 0; retry < HTTP_RETRY_ATTEMPTS; retry++) {
        if (retry > 0) {
            my_log_f("Geocode retry %d/%d...", retry + 1, HTTP_RETRY_ATTEMPTS);
            delay(HTTP_RETRY_DELAY_MS);
        }

        http.begin(url);
        http.setTimeout(HTTP_TIMEOUT_MS);

        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();

            // Parse JSON
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (!error && doc["results"].size() > 0) {
                latitude = doc["results"][0]["latitude"];
                longitude = doc["results"][0]["longitude"];
                String foundCity = doc["results"][0]["name"].as<String>();
                String country = doc["results"][0]["country"].as<String>();

                my_log_f("Found: %s, %s at %.4f, %.4f",
                             foundCity.c_str(), country.c_str(), latitude, longitude);

                http.end();
                return true;
            } else if (error) {
                my_log_f("JSON parsing error: %s", error.c_str());
            } else {
                my_log("No results found for city");
            }
        } else {
            my_log_f("HTTP error code: %d", httpCode);
        }

        http.end();
    }

    my_log("Geocoding failed after all retries");
    return false;
}
