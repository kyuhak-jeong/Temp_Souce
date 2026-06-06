#ifndef VPB_SUBTITLE_H
#define VPB_SUBTITLE_H

#include <string>
#include <sstream>
#include <cstdint>
#include <vector>
#include <cstring>
#include <ctime>
#include <chrono>
#include "constants.h"
#include "vpb_const.h"

namespace VPB
{

struct SubtitleData
{
    int64_t      timestamp;
    std::string  rawText;
    DateTimeData datetimeData;
    GPSData      gps;
    IMUData      imu;
    OBDData      obd;
    
    SubtitleData() : timestamp(0) {}
};

// ============================================================================
// Subtitle Parser
// ============================================================================

class SubtitleParser
{
public:
    static bool parse(const std::string& text, SubtitleData& data)
    {
        data.rawText = text;
        if (text.empty() || text.size() < 3) return false;
        
        std::string content = text;
        if (content.front() == '[') content = content.substr(1);
        if (content.back() == ']') content = content.substr(0, content.size() - 1);
        
        std::vector<std::string> tokens;
        std::istringstream iss(content);
        std::string token;
        while (iss >> token) tokens.push_back(token);
        
        if (tokens.empty()) return false;
        
        size_t currentIdx = 0;
        
        if (parseGPS(tokens, currentIdx, data.gps) == true)                    currentIdx += 2;
        else if (parseDateTime(tokens, currentIdx, data.datetimeData) == true) currentIdx += 1;

        parseIMU(tokens, currentIdx, data.imu);                                currentIdx += 6;
        parseOBD(tokens, currentIdx, data.obd);
        
        return true;
    }
    
private:
    static bool parseGPS(const std::vector<std::string>& tokens, size_t startIdx, GPSData& gps)
    {
        if (startIdx + 1 >= tokens.size())
        {
            gps.valid = false;
            return false;
        }
        
        const std::string& lat = tokens[startIdx];
        const std::string& lon = tokens[startIdx + 1];

        if (lat.find('.') == std::string::npos || lon.find('.') == std::string::npos)
        {
            gps.valid = false;
            return false;
        }
        
        if (lat == "N/A" || lon == "N/A")
        {
            gps.valid = false;
            return false;
        }
        
        try
        {
            gps.latitude = std::stof(lat);
            gps.longitude = std::stof(lon);
            gps.valid = true;
            return true;
        }
        catch (...)
        {
            gps.valid = false;
            return false;
        }
    }
    
    static bool parseDateTime(const std::vector<std::string>& tokens, size_t startIdx, 
                              DateTimeData& datetimeData)
    {
        if (startIdx >= tokens.size())
        {
            datetimeData.valid = false;
            return false;
        }
        
        const std::string& token = tokens[startIdx];
        
        // Check if token looks like datetime (has underscore, format: YYMMDD_HHMMSS)
        if (token.find('_') == std::string::npos || token.size() < 13)
        {
            datetimeData.valid = false;
            return false;
        }
        
        datetimeData.valid = true;
        datetimeData.datetime = formatDateTime(token);
        return true;
    }
    
    static std::string formatDateTime(const std::string& raw)
    {
        // Input: YYMMDD_HHMMSS
        // Output: 20YY-MM-DD HH:MM:SS
        
        if (raw.size() < 13) return raw;
        
        size_t underscorePos = raw.find('_');
        if (underscorePos == std::string::npos) return raw;
        
        std::string datePart = raw.substr(0, underscorePos);
        std::string timePart = raw.substr(underscorePos + 1);
        
        if (datePart.size() >= 6 && timePart.size() >= 6)
        {
            std::string yy = datePart.substr(0, 2);
            std::string mm = datePart.substr(2, 2);
            std::string dd = datePart.substr(4, 2);
            std::string hh = timePart.substr(0, 2);
            std::string mi = timePart.substr(2, 2);
            std::string ss = timePart.substr(4, 2);
            
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "20%s-%s-%s %s:%s:%s", 
                    yy.c_str(), mm.c_str(), dd.c_str(), hh.c_str(), mi.c_str(), ss.c_str());
            return std::string(buffer);
        }
        
        return raw;
    }
    
    static bool parseIMU(const std::vector<std::string>& tokens, size_t startIdx, IMUData& imu)
    {
        if (startIdx + 5 >= tokens.size())
        {
            imu.valid = false;
            return false;
        }
        
        for (size_t i = 0; i < 6; ++i)
        {
            if (tokens[startIdx + i] == "N/A")
            {
                imu.valid = false;
                return false;
            }
        }
        
        try
        {
            imu.accelX = std::stof(tokens[startIdx + 0]);
            imu.accelY = std::stof(tokens[startIdx + 1]);
            imu.accelZ = std::stof(tokens[startIdx + 2]);
            imu.gyroX = std::stof(tokens[startIdx + 3]);
            imu.gyroY = std::stof(tokens[startIdx + 4]);
            imu.gyroZ = std::stof(tokens[startIdx + 5]);
            imu.valid = true;
            return true;
        }
        catch (...)
        {
            imu.valid = false;
            return false;
        }
    }
    
    static bool parseOBD(const std::vector<std::string>& tokens, size_t startIdx, OBDData& obd)
    {
        if (startIdx + 4 >= tokens.size())
        {
            obd.valid = false;
            return false;
        }
        
        for (size_t i = 0; i < 5; ++i)
        {
            if (tokens[startIdx + i] == "N/A")
            {
                obd.valid = false;
                return false;
            }
        }
        
        try
        {
            obd.speedKmh = std::stoi(tokens[startIdx + 0]);
            obd.rpm = std::stoi(tokens[startIdx + 1]);
            obd.gearPos = std::stoi(tokens[startIdx + 2]);
            obd.turnSignal = std::stoi(tokens[startIdx + 3]);
            obd.steeringAngle = std::stoi(tokens[startIdx + 4]);
            obd.valid = true;
            return true;
        }
        catch (...)
        {
            obd.valid = false;
            return false;
        }
    }
};

// ============================================================================
// Subtitle Style Configuration
// ============================================================================

struct SubtitleStyle
{
    enum class Position { TOP_LEFT, TOP_CENTER, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_CENTER, BOTTOM_RIGHT, CUSTOM };
    
    int32_t  fontSize;
    float    fontColor[4];
    float    backgroundColor[4];
    float    outlineColor[4];
    int32_t  outlineThickness;
    Position position;
    int32_t  offsetX, offsetY, padding;
    bool     showTimestamp, showGPS, showDateTime, showIMU, showOBD, showLabels, showBackground, showOutline;
    
    SubtitleStyle() : fontSize(16), fontColor{1.0f, 1.0f, 1.0f, 1.0f}, backgroundColor{0.0f, 0.0f, 0.0f, 0.7f},
                      outlineColor{0.0f, 0.0f, 0.0f, 1.0f}, outlineThickness(1), position(Position::BOTTOM_CENTER),
                      offsetX(0), offsetY(0), padding(10), showTimestamp(false), showGPS(true), showDateTime(true),
                      showIMU(true), showOBD(true), showLabels(true), showBackground(true), showOutline(true) {}
};

// ============================================================================
// Subtitle Formatter
// ============================================================================

class SubtitleFormatter
{
public:
    static std::string format(const SubtitleData& data, const SubtitleStyle& style)
    {
        auto [line1, line2] = formatTwoLines(data, style);
        if (line1.empty() == true)  return line2;
        if (line2.empty() == true)  return line1;
        return line1 + "\n" + line2;
    }

    // Returns { line1: datetime + OBD,  line2: GPS + IMU }.
    // Either string may be empty if the relevant data is absent or disabled.
    static std::pair<std::string, std::string> formatTwoLines(const SubtitleData& data, const SubtitleStyle& style)
    {
        std::ostringstream line1, line2;

        // ── Line 1: optional timestamp prefix, then datetime, then OBD ──────
        if (style.showTimestamp == true)
        {
            line1 << "[" << VPB::TimeUtils::formatTime(data.timestamp) << "]";
            bool hasMore = (style.showDateTime == true && data.datetimeData.valid == true)
                        || (style.showOBD      == true && data.obd.valid          == true);
            if (hasMore == true) line1 << " ";
        }

        if (style.showDateTime == true && data.datetimeData.valid == true)
        {
            line1 << data.datetimeData.datetime;
            if (style.showOBD == true && data.obd.valid == true) line1 << " | ";
        }

        if (style.showOBD == true && data.obd.valid == true)
            line1 << formatOBD(data.obd);

        // ── Line 2: GPS, then IMU ────────────────────────────────────────────
        if (style.showGPS == true && data.gps.valid == true)
        {
            if (style.showLabels == true) line2 << "GPS: ";
            line2 << formatGPS(data.gps);
            if (style.showIMU == true && data.imu.valid == true) line2 << " | ";
        }

        if (style.showIMU == true && data.imu.valid == true)
            line2 << formatIMU(data.imu);

        return { line1.str(), line2.str() };
    }
    
private:
    static std::string formatGPS(const GPSData& gps)
    {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "Lat:%+011.06f Lon:%+011.06f", gps.latitude, gps.longitude);
        return std::string(buffer);
    }
    
    static std::string formatIMU(const IMUData& imu)
    {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "Accel[%03.03f %03.03f %03.03f] Gyro[%03.03f %03.03f %03.03f]",
                 imu.accelX, imu.accelY, imu.accelZ, imu.gyroX, imu.gyroY, imu.gyroZ);
        return std::string(buffer);
    }
    
    static std::string formatOBD(const OBDData& obd)
    {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "Speed:%dkm/h RPM:%d Gear:%s Turn:%s Steer:%d deg",
                 obd.speedKmh, obd.rpm, formatGear(obd.gearPos).c_str(),
                 formatTurnSignal(obd.turnSignal).c_str(), obd.steeringAngle);
        return std::string(buffer);
    }
    
    static std::string formatGear(int32_t gear)
    {
        if (gear == -1) return "R";
        if (gear == 0) return "N";
        if (gear == 1) return "D";
        if (gear == 1) return "P";
        return std::to_string(gear);
    }
    
    static std::string formatTurnSignal(int32_t signal)
    {
        switch (signal)
        {
            case 0: return "OFF";
            case 1: return "<<<";
            case 2: return ">>>";
            case 3: return "!!!";
            default: return "???";
        }
    }
};

} // namespace VPB

#endif // VPB_SUBTITLE_H
