#ifndef __WEATHER_H__
#define __WEATHER_H__

#include <cstdint>
#include <memory>
#include <vector>

#ifdef ARDUINO
#include "WString.h"
#else
#include <string>
using String = std::string;
#endif

struct HourlyWeather {
    String fxTime;
    int32_t temp{0};
    int32_t humidity{0};
    float precip{0.0f};
    int32_t pressure{0};
    int32_t pop{0};
    int32_t wind360{0};
    int32_t windSpeed{0};
    String windDir;
    String icon;
    String text;
};

struct DailyWeather {
    String fxDate;
    int32_t tempMax{0};
    int32_t tempMin{0};
    int32_t humidity{0};
    float precip{0.0f};
    int32_t pressure{0};
    int32_t uvIndex{0};
    String iconDay;
    String iconNight;
    String textDay;
    String textNight;
    String sunrise;
    String sunset;
    String moonrise;
    String moonset;
    String moonPhase;
};

struct Weather {
    int32_t temp{0};
    int32_t feelsLike{0};
    int32_t humidity{0};
    float precip{0.0f};
    int32_t pressure{0};
    int32_t wind360{0};
    int32_t windSpeed{0};
    String windDir;
    String icon;
    String text;
    String obsTime;
};

class WeatherManager {
private:
    String hefeng_proxy_token_;
    String hefeng_host_;
    String city_id_;
    bool use_metric{true};

    std::shared_ptr<Weather> currentWeather;
    std::shared_ptr<std::vector<HourlyWeather>> hourlyWeathers;
    std::shared_ptr<std::vector<DailyWeather>> dailyWeathers;

    String buildUrl(const String& endpoint, const String& params = "");
    bool parseResponseCode(const String& json);

#ifdef ARDUINO
    String httpGet(const String& url);
#endif

public:
    WeatherManager(const String& cityId, const String& proxy_token, const String& host);

    void setMetricUnit(bool metric) { use_metric = metric; }
    bool isMetricUnit() const { return use_metric; }

    std::shared_ptr<Weather> requestWeatherNow();
    std::shared_ptr<std::vector<HourlyWeather>> requestHourlyForecasts(int hours = 24);
    std::shared_ptr<std::vector<DailyWeather>> requestDailyForecasts(int days = 7);

    std::shared_ptr<Weather> getCurrentWeather() const { return currentWeather; }
    std::shared_ptr<std::vector<HourlyWeather>> getHourlyWeathers() const { return hourlyWeathers; }
    std::shared_ptr<std::vector<DailyWeather>> getDailyWeathers() const { return dailyWeathers; }

    static const char* getErrorMessage(const String& code);
};

#endif // __WEATHER_H__
