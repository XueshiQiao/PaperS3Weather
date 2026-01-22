#include "weather.h"
#include "Logger.h"

#ifdef ARDUINO
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#endif

WeatherManager::WeatherManager(const String& cityId, const String& proxy_token, const String& host)
    : city_id_(cityId), hefeng_proxy_token_(proxy_token), hefeng_host_(host) {}

String WeatherManager::buildUrl(const String& endpoint, const String& params) {
    String url = hefeng_host_ + endpoint + "?location=" + city_id_ + "&lang=en";
    if (params.length() > 0) {
        url += "&" + params;
    }
    url += "&unit=" + String(use_metric ? "m" : "i");
    return url;
}

bool WeatherManager::parseResponseCode(const String& json) {
#ifdef ARDUINO
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        my_log_f("JSON parse error: %s", error.c_str());
        return false;
    }
    String code = doc["code"].as<String>();
    if (code != "200") {
        my_log_f("API error code: %s - %s", code.c_str(), getErrorMessage(code));
        return false;
    }
    return true;
#else
    return json.find("\"code\":\"200\"") != std::string::npos;
#endif
}

const char* WeatherManager::getErrorMessage(const String& code) {
    if (code == "200") return "Success";
    if (code == "204") return "No data";
    if (code == "400") return "Bad request";
    if (code == "401") return "Unauthorized";
    if (code == "402") return "Payment required";
    if (code == "403") return "Forbidden";
    if (code == "404") return "Not found";
    if (code == "429") return "Too many requests";
    if (code == "500") return "Server error";
    return "Unknown error";
}

#ifdef ARDUINO
String WeatherManager::httpGet(const String& url) {
    if (WiFi.status() != WL_CONNECTED) {
        my_log("WiFi not connected");
        return "";
    }

    HTTPClient http;
    String requestUrl = url;

    http.begin(requestUrl);
    http.setTimeout(10000);

    http.addHeader("X-Proxy-Token", hefeng_proxy_token_);

    int httpCode = http.GET();
    String payload = "";

    if (httpCode == HTTP_CODE_OK) {
        payload = http.getString();
        my_log_f("HTTP OK, payload size: %d", payload.length());
    } else {
        payload = http.getString();
        my_log_f("HTTP error: %d, payload: %s", httpCode, payload.c_str());
    }

    http.end();
    return payload;
}

std::shared_ptr<Weather> WeatherManager::requestWeatherNow() {
    String url = buildUrl("/v7/weather/now");
    my_log("Requesting weather now: " + url);

    String payload = httpGet(url);
    if (payload.length() == 0) {
        return nullptr;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        my_log_f("JSON parse error: %s", error.c_str());
        my_log_f("Payload length: %d", payload.length());
        if (payload.length() > 2) {
            my_log_f("First 3 bytes (hex): %02X %02X %02X",
                     (unsigned char)payload[0],
                     (unsigned char)payload[1],
                     (unsigned char)payload[2]);
        }
        return nullptr;
    }

    String code = doc["code"].as<String>();
    if (code != "200") {
        my_log_f("API error: %s - %s", code.c_str(), getErrorMessage(code));
        return nullptr;
    }

    currentWeather = std::make_shared<Weather>();
    JsonObject now = doc["now"];

    currentWeather->temp = now["temp"].as<int32_t>();
    currentWeather->feelsLike = now["feelsLike"].as<int32_t>();
    currentWeather->humidity = now["humidity"].as<int32_t>();
    currentWeather->precip = now["precip"].as<float>();
    currentWeather->pressure = now["pressure"].as<int32_t>();
    currentWeather->wind360 = now["wind360"].as<int32_t>();
    currentWeather->windSpeed = now["windSpeed"].as<int32_t>();
    currentWeather->windDir = now["windDir"].as<String>();
    currentWeather->icon = now["icon"].as<String>();
    currentWeather->text = now["text"].as<String>();
    currentWeather->obsTime = now["obsTime"].as<String>();

    my_log("=== QWeather Current Conditions ===");
    my_log_f("Temperature: %d", currentWeather->temp);
    my_log_f("Feels Like: %d", currentWeather->feelsLike);
    my_log_f("Humidity: %d%%", currentWeather->humidity);
    my_log_f("Wind: %d km/h @ %d°", currentWeather->windSpeed, currentWeather->wind360);
    my_log_f("Condition: %s", currentWeather->text.c_str());

    return currentWeather;
}

std::shared_ptr<std::vector<HourlyWeather>> WeatherManager::requestHourlyForecasts(int hours) {
    String endpoint;
    if (hours <= 24) {
        endpoint = "/v7/weather/24h";
    } else if (hours <= 72) {
        endpoint = "/v7/weather/72h";
    } else {
        endpoint = "/v7/weather/168h";
    }

    String url = buildUrl(endpoint);
    my_log("Requesting hourly forecast: " + url);

    String payload = httpGet(url);
    if (payload.length() == 0) {
        return nullptr;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        my_log_f("JSON parse error: %s", error.c_str());
        return nullptr;
    }

    String code = doc["code"].as<String>();
    if (code != "200") {
        my_log_f("API error: %s - %s", code.c_str(), getErrorMessage(code));
        return nullptr;
    }

    hourlyWeathers = std::make_shared<std::vector<HourlyWeather>>();
    JsonArray hourlyArray = doc["hourly"];

    int count = 0;
    for (JsonObject item : hourlyArray) {
        if (count >= hours) break;

        HourlyWeather hw;
        hw.fxTime = item["fxTime"].as<String>();
        hw.temp = item["temp"].as<int32_t>();
        hw.humidity = item["humidity"].as<int32_t>();
        hw.precip = item["precip"].as<float>();
        hw.pressure = item["pressure"].as<int32_t>();
        hw.pop = item["pop"].as<int32_t>();
        hw.wind360 = item["wind360"].as<int32_t>();
        hw.windSpeed = item["windSpeed"].as<int32_t>();
        hw.windDir = item["windDir"].as<String>();
        hw.icon = item["icon"].as<String>();
        hw.text = item["text"].as<String>();

        hourlyWeathers->push_back(hw);
        count++;
    }

    my_log_f("Parsed %d hourly forecasts", count);
    return hourlyWeathers;
}

std::shared_ptr<std::vector<DailyWeather>> WeatherManager::requestDailyForecasts(int days) {
    String endpoint;
    if (days <= 3) {
        endpoint = "/v7/weather/3d";
    } else if (days <= 7) {
        endpoint = "/v7/weather/7d";
    } else if (days <= 10) {
        endpoint = "/v7/weather/10d";
    } else if (days <= 15) {
        endpoint = "/v7/weather/15d";
    } else {
        endpoint = "/v7/weather/30d";
    }

    String url = buildUrl(endpoint);
    my_log("Requesting daily forecast: " + url);

    String payload = httpGet(url);
    if (payload.length() == 0) {
        return nullptr;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        my_log_f("JSON parse error: %s", error.c_str());
        return nullptr;
    }

    String code = doc["code"].as<String>();
    if (code != "200") {
        my_log_f("API error: %s - %s", code.c_str(), getErrorMessage(code));
        return nullptr;
    }

    dailyWeathers = std::make_shared<std::vector<DailyWeather>>();
    JsonArray dailyArray = doc["daily"];

    int count = 0;
    for (JsonObject item : dailyArray) {
        if (count >= days) break;

        DailyWeather dw;
        dw.fxDate = item["fxDate"].as<String>();
        dw.tempMax = item["tempMax"].as<int32_t>();
        dw.tempMin = item["tempMin"].as<int32_t>();
        dw.humidity = item["humidity"].as<int32_t>();
        dw.precip = item["precip"].as<float>();
        dw.pressure = item["pressure"].as<int32_t>();
        dw.uvIndex = item["uvIndex"].as<int32_t>();
        dw.iconDay = item["iconDay"].as<String>();
        dw.iconNight = item["iconNight"].as<String>();
        dw.textDay = item["textDay"].as<String>();
        dw.textNight = item["textNight"].as<String>();
        dw.sunrise = item["sunrise"].as<String>();
        dw.sunset = item["sunset"].as<String>();
        dw.moonrise = item["moonrise"].as<String>();
        dw.moonset = item["moonset"].as<String>();
        dw.moonPhase = item["moonPhase"].as<String>();

        dailyWeathers->push_back(dw);
        count++;
    }

    my_log_f("Parsed %d daily forecasts", count);

    if (count > 0) {
        my_log("=== QWeather Daily Forecast (Day 1) ===");
        my_log_f("Date: %s", (*dailyWeathers)[0].fxDate.c_str());
        my_log_f("Temp: %d / %d", (*dailyWeathers)[0].tempMax, (*dailyWeathers)[0].tempMin);
        my_log_f("Sunrise: %s, Sunset: %s",
                 (*dailyWeathers)[0].sunrise.c_str(),
                 (*dailyWeathers)[0].sunset.c_str());
    }

    return dailyWeathers;
}

#else
std::shared_ptr<Weather> WeatherManager::requestWeatherNow() {
    return nullptr;
}

std::shared_ptr<std::vector<HourlyWeather>> WeatherManager::requestHourlyForecasts(int hours) {
    (void)hours;
    return nullptr;
}

std::shared_ptr<std::vector<DailyWeather>> WeatherManager::requestDailyForecasts(int days) {
    (void)days;
    return nullptr;
}
#endif
