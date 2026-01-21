#include "display.h"
#include "constants.h"
#include "utils.h"
#include "Icons.h"
#include <WiFi.h>
#include "Logger.h"

extern WeatherData currentWeather;
// canvas is now extern from utils.h/main.cpp
extern String cityName;

void drawIcon(int x, int y, const uint8_t *icon, int dx, int dy, bool highContrast, float scale) {
    const uint16_t *icon16 = (const uint16_t *)icon;
    int scaledDX = (int)(dx * scale);
    int scaledDY = (int)(dy * scale);

    for (int yi = 0; yi < scaledDY; yi++) {
        for (int xi = 0; xi < scaledDX; xi++) {
            // Map scaled coordinates back to source coordinates
            int srcX = (int)(xi / scale);
            int srcY = (int)(yi / scale);

            if (srcX >= dx) srcX = dx - 1;
            if (srcY >= dy) srcY = dy - 1;

            uint16_t pixel = icon16[srcY * dx + srcX];
            int grayscale = 15 - (pixel / ICON_GRAYSCALE_DIVISOR);

            if (highContrast) {
                if (grayscale > 0) {
                    canvas.drawPixel(x + xi, y + yi, TFT_BLACK);
                }
            } else {
                uint16_t color = 0xFFFF - (grayscale * ICON_COLOR_MULTIPLIER);
                canvas.drawPixel(x + xi, y + yi, color);
            }
        }
    }
}

void drawRSSI(int x, int y, int rssi) {
    int quality = getRSSIQuality(rssi);

    auto drawArc = [&](int cx, int cy, int r, int fromDeg, int toDeg) {
        for (int i = fromDeg; i < toDeg; i++) {
            double rad = i * PI / 180;
            int px = cx + r * cos(rad);
            int py = cy + r * sin(rad);
            canvas.drawPixel(px, py, TFT_BLACK);
        }
    };

    if (quality >= 80) drawArc(x + 12, y, 16, 225, 315);
    if (quality >= 40) drawArc(x + 12, y, 12, 225, 315);
    if (quality >= 20) drawArc(x + 12, y, 8, 225, 315);
    if (quality >= 10) drawArc(x + 12, y, 4, 225, 315);
    drawArc(x + 12, y, 2, 225, 315);
}

void drawBattery(int x, int y, int batteryPercent) {
    canvas.drawRect(x, y, BATTERY_WIDTH, BATTERY_HEIGHT, TFT_BLACK);
    canvas.drawRect(x + BATTERY_WIDTH, y + BATTERY_TIP_OFFSET, BATTERY_TIP_WIDTH, BATTERY_TIP_HEIGHT, TFT_BLACK);

    // Fill battery based on percentage
    for (int i = x; i < x + BATTERY_WIDTH; i++) {
        canvas.drawLine(i, y, i, y + BATTERY_HEIGHT - 1, TFT_BLACK);
        if ((i - x) * 100.0 / BATTERY_WIDTH > batteryPercent) {
            break;
        }
    }
}

void drawArrow(int x, int y, int asize, float aangle, int pwidth, int plength) {
    float dx = (asize + 21) * cos((aangle - 90) * PI / 180) + x;
    float dy = (asize + 21) * sin((aangle - 90) * PI / 180) + y;
    float x1 = 0;           float y1 = plength;
    float x2 = pwidth / 2;  float y2 = pwidth / 2;
    float x3 = -pwidth / 2; float y3 = pwidth / 2;
    float angle = aangle * PI / 180;
    float xx1 = x1 * cos(angle) - y1 * sin(angle) + dx;
    float yy1 = y1 * cos(angle) + x1 * sin(angle) + dy;
    float xx2 = x2 * cos(angle) - y2 * sin(angle) + dx;
    float yy2 = y2 * cos(angle) + x2 * sin(angle) + dy;
    float xx3 = x3 * cos(angle) - y3 * sin(angle) + dx;
    float yy3 = y3 * cos(angle) + x3 * sin(angle) + dy;
    canvas.fillTriangle(xx1, yy1, xx3, yy3, xx2, yy2, TFT_BLACK);
}

void drawWindCompass(int x, int y, float angle, float windspeed, int radius) {
    int dxo, dyo, dxi, dyi;

    canvas.setTextSize(2);
    canvas.drawCircle(x, y, radius, TFT_BLACK);
    canvas.drawCircle(x, y, radius + 1, TFT_BLACK);
    canvas.drawCircle(x, y, radius * 0.7, TFT_BLACK);

    // Draw compass ticks
    for (float a = 0; a < 360; a += 22.5) {
        dxo = radius * cos((a - 90) * PI / 180);
        dyo = radius * sin((a - 90) * PI / 180);

        dxi = dxo * 0.9;
        dyi = dyo * 0.9;
        canvas.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, TFT_BLACK);

        dxo = dxo * 0.7;
        dyo = dyo * 0.7;
        dxi = dxo * 0.9;
        dyi = dyo * 0.9;
        canvas.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, TFT_BLACK);
    }

    // Draw cardinal directions
    int labelOffset = radius + COMPASS_LABEL_OFFSET;
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString("N", x, y - labelOffset);
    canvas.drawString("S", x, y + labelOffset - 8);

    canvas.setTextDatum(MC_DATUM);
    canvas.drawString("W", x - labelOffset, y);
    canvas.drawString("E", x + labelOffset, y);

    // Draw intercardinal directions
    int diagOffset = (int)(labelOffset * COMPASS_DIAG_FACTOR);
    canvas.setTextDatum(BR_DATUM);
    canvas.drawString("NE", x + diagOffset + 10, y - diagOffset);
    canvas.setTextDatum(TR_DATUM);
    canvas.drawString("SE", x + diagOffset + 10, y + diagOffset);
    canvas.setTextDatum(TL_DATUM);
    canvas.drawString("SW", x - diagOffset - 10, y + diagOffset);
    canvas.setTextDatum(BL_DATUM);
    canvas.drawString("NW", x - diagOffset - 10, y - diagOffset);

    // Draw wind speed
    String speedUnit = useCelsius ? "km/h" : "mph";
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString(String(windspeed, 1), x, y - 20);
    canvas.drawString(speedUnit, x, y);
    canvas.setTextDatum(TL_DATUM);

    // Draw wind direction arrow
    drawArrow(x, y, radius - 17, angle, COMPASS_ARROW_SIZE, COMPASS_ARROW_LENGTH);
}

void drawHourlyForecast(int x, int y, int dx, int dy, int index) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        timeinfo.tm_hour = 0;
    }
    int forecastHour = (timeinfo.tm_hour + index + 1) % 24;

    canvas.setTextSize(2);
    canvas.setTextDatum(TC_DATUM);
    char hourStr[6];
    sprintf(hourStr, "%02d:00", forecastHour);
    canvas.drawString(hourStr, x + dx / 2, y + 10);
    canvas.drawString(formatTemp(currentWeather.hourly[index].temp), x + dx / 2, y + 30);
    canvas.setTextDatum(TL_DATUM);

    bool isDay = isDaytime(forecastHour);
    int iconX = x + dx / 2 - 32;
    int iconY = y + 50;
    const uint8_t* weatherIcon = getWeatherIcon(currentWeather.hourly[index].weatherCode, isDay);
    drawIcon(iconX, iconY, weatherIcon, WEATHER_ICON_SIZE, WEATHER_ICON_SIZE, true);
}

void drawGraph(int x, int y, int dx, int dy, String title, int xMin, int xMax, float yMin, float yMax, float values[]) {
    String yMinString = String((int)yMin);
    String yMaxString = String((int)yMax);
    int textWidth = 5 + max(yMinString.length() * GRAPH_TEXT_WIDTH_FACTOR, yMaxString.length() * GRAPH_TEXT_WIDTH_FACTOR);

    int graphX = x + 5 + textWidth + 5;
    int graphY = y + GRAPH_AREA_Y_OFFSET;
    int graphDX = dx - textWidth - GRAPH_SIDE_MARGIN;
    int graphDY = dy - GRAPH_AREA_Y_OFFSET - GRAPH_BOTTOM_MARGIN;
    float xStep = graphDX / (float)(xMax - xMin);
    float yStep = graphDY / (yMax - yMin);

    // Draw title
    canvas.setTextSize(2);
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString(title, x + dx / 2, y + GRAPH_TITLE_Y_OFFSET);
    canvas.setTextDatum(TL_DATUM);

    // Draw Y-axis labels
    canvas.setTextSize(1);
    canvas.drawString(yMaxString, x + 5, graphY - 5);
    canvas.drawString(yMinString, x + 5, graphY + graphDY - 3);

    // Draw X-axis labels
    for (int i = 0; i <= (xMax - xMin); i++) {
        canvas.drawString(String(i), graphX + i * xStep, graphY + graphDY + 5);
    }

    // Draw graph border
    canvas.drawRect(graphX, graphY, graphDX, graphDY, TFT_BLACK);

    // Draw zero line if applicable
    if (yMin < 0 && yMax > 0) {
        float yValueDX = (float)graphDY / (yMax - yMin);
        int yPos = graphY + graphDY - (0.0 - yMin) * yValueDX;
        if (yPos > graphY && yPos < graphY + graphDY) {
            canvas.drawString("0", graphX - 20, yPos);
            for (int xDash = graphX; xDash < graphX + graphDX - GRAPH_DASH_SPACING; xDash += GRAPH_DASH_SPACING) {
                canvas.drawLine(xDash, yPos, xDash + GRAPH_DASH_LENGTH, yPos, TFT_BLACK);
            }
        }
    }

    // Plot data points and lines
    int lastX = -1, lastY = -1;
    for (int i = xMin; i <= xMax; i++) {
        float yValue = values[i - xMin];
        float yValueDY = (float)graphDY / (yMax - yMin);
        int xPos = graphX + graphDX / (xMax - xMin) * i;
        int yPos = graphY + graphDY - (yValue - yMin) * yValueDY;

        // Clamp to graph bounds
        if (yPos > graphY + graphDY) yPos = graphY + graphDY;
        if (yPos < graphY) yPos = graphY;

        canvas.fillCircle(xPos, yPos, GRAPH_POINT_RADIUS, TFT_BLACK);
        if (i > xMin) {
            canvas.drawLine(lastX, lastY, xPos, yPos, TFT_BLACK);
        }
        lastX = xPos;
        lastY = yPos;
    }
}

void drawTempGraph(int x, int y, int dx, int dy, String title, int xMin, int xMax, float yMin, float yMax, float highValues[], float lowValues[]) {
    String yMinString = String((int)yMin);
    String yMaxString = String((int)yMax);
    int textWidth = 5 + max(yMinString.length() * GRAPH_TEXT_WIDTH_FACTOR, yMaxString.length() * GRAPH_TEXT_WIDTH_FACTOR);

    int graphX = x + 5 + textWidth + 5;
    int graphY = y + GRAPH_AREA_Y_OFFSET;
    int graphDX = dx - textWidth - GRAPH_SIDE_MARGIN;
    int graphDY = dy - GRAPH_AREA_Y_OFFSET - GRAPH_BOTTOM_MARGIN;
    float xStep = graphDX / (float)(xMax - xMin);
    float yStep = graphDY / (yMax - yMin);

    // Draw title
    canvas.setTextSize(2);
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString(title, x + dx / 2, y + GRAPH_TITLE_Y_OFFSET);
    canvas.setTextDatum(TL_DATUM);

    // Draw Y-axis labels
    canvas.setTextSize(1);
    canvas.drawString(yMaxString, x + 5, graphY - 5);
    canvas.drawString(yMinString, x + 5, graphY + graphDY - 3);

    // Draw X-axis labels
    for (int i = 0; i <= (xMax - xMin); i++) {
        canvas.drawString(String(i), graphX + i * xStep, graphY + graphDY + 5);
    }

    // Draw graph border
    canvas.drawRect(graphX, graphY, graphDX, graphDY, TFT_BLACK);

    // Plot high temperatures
    int lastHighX = -1, lastHighY = -1;
    for (int i = xMin; i <= xMax; i++) {
        float yValue = highValues[i - xMin];
        float yValueDY = (float)graphDY / (yMax - yMin);
        int xPos = graphX + graphDX / (xMax - xMin) * i;
        int yPos = graphY + graphDY - (yValue - yMin) * yValueDY;

        if (yPos > graphY + graphDY) yPos = graphY + graphDY;
        if (yPos < graphY) yPos = graphY;

        canvas.fillCircle(xPos, yPos, GRAPH_POINT_RADIUS, TFT_BLACK);
        if (i > xMin) {
            canvas.drawLine(lastHighX, lastHighY, xPos, yPos, TFT_BLACK);
        }
        lastHighX = xPos;
        lastHighY = yPos;
    }

    // Plot low temperatures
    int lastLowX = -1, lastLowY = -1;
    for (int i = xMin; i <= xMax; i++) {
        float yValue = lowValues[i - xMin];
        float yValueDY = (float)graphDY / (yMax - yMin);
        int xPos = graphX + graphDX / (xMax - xMin) * i;
        int yPos = graphY + graphDY - (yValue - yMin) * yValueDY;

        if (yPos > graphY + graphDY) yPos = graphY + graphDY;
        if (yPos < graphY) yPos = graphY;

        canvas.fillCircle(xPos, yPos, GRAPH_POINT_RADIUS, TFT_BLACK);
        if (i > xMin) {
            canvas.drawLine(lastLowX, lastLowY, xPos, yPos, TFT_BLACK);
        }
        lastLowX = xPos;
        lastLowY = yPos;
    }

    // Plot average temperature (dotted line)
    int lastAvgX = -1, lastAvgY = -1;
    for (int i = xMin; i <= xMax; i++) {
        float avgValue = (highValues[i - xMin] + lowValues[i - xMin]) / 2.0;
        float yValueDY = (float)graphDY / (yMax - yMin);
        int xPos = graphX + graphDX / (xMax - xMin) * i;
        int yPos = graphY + graphDY - (avgValue - yMin) * yValueDY;

        if (yPos > graphY + graphDY) yPos = graphY + graphDY;
        if (yPos < graphY) yPos = graphY;

        canvas.fillCircle(xPos, yPos, 1, TFT_BLACK);
        if (i > xMin) {
            // Draw dotted line
            int dx = xPos - lastAvgX;
            int dy = yPos - lastAvgY;
            float len = sqrt(dx*dx + dy*dy);
            for (float t = 0; t < len; t += 5) {
                int px = lastAvgX + (dx * t / len);
                int py = lastAvgY + (dy * t / len);
                canvas.drawPixel(px, py, TFT_BLACK);
            }
        }
        lastAvgX = xPos;
        lastAvgY = yPos;
    }
}

void drawBigClock(int x, int y, int dx, int dy) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;

    char timeStr[6];
    sprintf(timeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

    // Draw Time centered in the full panel
    canvas.setFont(&fonts::FreeSansBold24pt7b);
    canvas.setTextSize(3);
    canvas.setTextDatum(MC_DATUM);

    // Center of the panel + offset
    int contentCenterY = y + (dy / 2) + 15; // Moved down 15px
    canvas.drawString(timeStr, x + dx / 2, contentCenterY);

    canvas.setFont(nullptr);
    canvas.setTextDatum(TL_DATUM);
}

void drawCurrentConditions(int x, int y, int dx, int dy) {
    // Layout Constants
    int centerLine = x + dx / 2;
    int contentCenterY = y + (dy / 2) + 35; // Moved content center down further (was +15)

    // --- ROW 1: Icon & Temp (Side by Side) ---
    // Icon Left, Temp Right. Bottom aligned.

    canvas.setFont(&fonts::FreeSansBold24pt7b);
    canvas.setTextSize(2);
    String tempNum = String((int)currentWeather.temperature);
    int tempWidth = canvas.textWidth(tempNum);

    // Scale icon to match temperature height (~96px)
    float iconScale = 1.5;
    int scaledIconSize = (int)(WEATHER_ICON_SIZE * iconScale);
    int gap = 20;

    // Total width to center the group
    int totalBlockWidth = scaledIconSize + gap + tempWidth + 15; // +15 for degree symbol space

    int startX = centerLine - (totalBlockWidth / 2);
    // Move the block up slightly to leave room for text below
    int blockBottomY = contentCenterY - 20;

    // 1. Draw Icon (Left)
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) timeinfo.tm_hour = 12;
    bool isDay = isDaytime(timeinfo.tm_hour);
    const uint8_t* weatherIcon = getWeatherIcon(currentWeather.weatherCode, isDay);

    // Icon drawn from top-left.
    drawIcon(startX, blockBottomY - scaledIconSize, weatherIcon, WEATHER_ICON_SIZE, WEATHER_ICON_SIZE, true, iconScale);

    // 2. Draw Temp (Right of Icon, Bottom Aligned)
    canvas.setTextDatum(BL_DATUM);
    canvas.drawString(tempNum, startX + scaledIconSize + gap, blockBottomY + 8); // +8 visual adjustment for font baseline

    // 3. Draw Degree Symbol
    drawDegreeSymbol(startX + scaledIconSize + gap + tempWidth + 8, blockBottomY - 65, TEMP_DEGREE_RADIUS_LARGE);

    canvas.setFont(nullptr);
    canvas.setTextFont(1);

    // --- ROW 2: Condition Text ---
    canvas.setTextSize(3);
    canvas.setTextDatum(TC_DATUM);
    String condition = getWeatherConditionText(currentWeather.weatherCode);

    if (canvas.textWidth(condition) > dx - 20) canvas.setTextSize(2);

    // Position below the icon/temp block
    canvas.drawString(condition, centerLine, blockBottomY + 25);

    // --- ROW 3: Details (Feels Like | High/Low) ---
    int detailsY = y + dy - 30; // Near bottom
    int quarterLeft = x + dx / 4;
    int quarterRight = x + (dx * 3) / 4;

    String feelsStr = "Feels " + String((int)currentWeather.apparentTemperature);
    String rangeStr = "H:" + String((int)currentWeather.todayMaxTemp) + " L:" + String((int)currentWeather.todayMinTemp);

    canvas.setTextDatum(TC_DATUM);
    canvas.setTextSize(2);

    // Left side
    canvas.drawString(feelsStr, quarterLeft, detailsY);
    drawDegreeSymbol(quarterLeft + canvas.textWidth(feelsStr)/2 + 5, detailsY - 8, FEELS_LIKE_DEGREE_RADIUS);

    // Right side
    canvas.drawString(rangeStr, quarterRight, detailsY);
}

void displayWeather() {
    my_log("displayWeather: Waking up display...");
    M5.Display.wakeup();
    delay(100);
    M5.Display.setRotation(1);

    // Set update mode for text and graphics
    M5.Display.setEpdMode(epd_mode_t::epd_text);

    M5.Display.startWrite();

    my_log("displayWeather: Clearing screen...");
    // Direct Draw: Clear the screen first (replaces fillSprite)
    canvas.fillScreen(TFT_WHITE);
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextSize(2);

    my_log("displayWeather: Drawing UI elements...");

    // Draw header
    canvas.setTextSize(2);
    canvas.drawString(VERSION, 20, 10);
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString(cityName, SCREEN_WIDTH / 2, 10);
    canvas.setTextDatum(TL_DATUM);

    // Draw WiFi signal strength
    int rssi = WiFi.RSSI();
    int quality = getRSSIQuality(rssi);
    canvas.setTextDatum(TR_DATUM);
    canvas.drawString(String(quality) + "%", SCREEN_WIDTH - 153, 10);
    canvas.setTextDatum(TL_DATUM);
    drawRSSI(SCREEN_WIDTH - 147, 23, rssi);

    // Draw battery level
    int batteryPercent = M5.Power.getBatteryLevel();
    if (batteryPercent < 0) batteryPercent = 0;
    if (batteryPercent > 100) batteryPercent = 100;

    canvas.setTextDatum(TR_DATUM);
    canvas.drawString(String(batteryPercent) + "%", SCREEN_WIDTH - 71, 10);
    canvas.setTextDatum(TL_DATUM);
    drawBattery(SCREEN_WIDTH - 60, 10, batteryPercent);

    // Draw config button indicator
    canvas.setTextSize(1);
    canvas.drawString("[CFG]", SCREEN_WIDTH - 50, SCREEN_HEIGHT - 20);

    // Draw main border
    canvas.drawRect(PANEL_BORDER, HEADER_HEIGHT, SCREEN_WIDTH - 28, SCREEN_HEIGHT - 43, TFT_BLACK);

    // Draw top row panels (Big Clock on Left, Weather on Right)
    int totalWidth = SCREEN_WIDTH - 30;
    int halfWidth = totalWidth / 2;

    canvas.drawRect(PANEL_SPACING, PANEL_TITLE_HEIGHT, totalWidth, 251, TFT_BLACK);
    canvas.drawLine(PANEL_SPACING + halfWidth, PANEL_TITLE_HEIGHT, PANEL_SPACING + halfWidth, 286, TFT_BLACK);

    drawBigClock(PANEL_SPACING, PANEL_TITLE_HEIGHT, halfWidth, 251);
    drawCurrentConditions(PANEL_SPACING + halfWidth, PANEL_TITLE_HEIGHT, halfWidth, 251);

    // Draw hourly forecast row
    canvas.drawRect(PANEL_SPACING, 286, SCREEN_WIDTH - 30, 122, TFT_BLACK);
    for (int i = 0; i < MAX_HOURLY; i++) {
        int x = PANEL_SPACING + i * 116;
        canvas.drawLine(x, 286, x, 408, TFT_BLACK);
        drawHourlyForecast(x, 286, 116, 122, i);
    }

    // Draw graphs row
    canvas.drawRect(PANEL_SPACING, 408, SCREEN_WIDTH - 30, 122, TFT_BLACK);

    float hourlyUVArray[MAX_HOURLY];
    float hourlyPrecipArray[MAX_HOURLY];
    float hourlyHumidityArray[MAX_HOURLY];
    float hourlyPressureArray[MAX_HOURLY];

    for (int i = 0; i < MAX_HOURLY; i++) {
        hourlyUVArray[i] = currentWeather.hourly[i].uvIndex;
        hourlyPrecipArray[i] = currentWeather.hourly[i].precip;
        hourlyHumidityArray[i] = currentWeather.hourly[i].humidity;
        hourlyPressureArray[i] = currentWeather.hourly[i].pressure;
    }

    drawGraph(PANEL_SPACING, 408, 232, 122, "UV Index", 0, 7, 0, 12, hourlyUVArray);
    drawGraph(247, 408, 232, 122, "Precip (%)", 0, 7, 0, 100, hourlyPrecipArray);
    drawGraph(479, 408, 232, 122, "Humidity (%)", 0, 7, 0, 100, hourlyHumidityArray);
    drawGraph(711, 408, 232, 122, "Pressure (hPa)", 0, 7, 980, 1040, hourlyPressureArray);


    M5.Display.endWrite();
    my_log("displayWeather: Triggering EPD refresh...");
    M5.Display.display();

    // WAIT for the E-Ink refresh to complete using the standard API
    my_log("displayWeather: Waiting for display refresh...");
    // may takes 1 seconds for deep refresh
    M5.Display.waitDisplay();
    my_log("displayWeather: Refresh complete.");
}
