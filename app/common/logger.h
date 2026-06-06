#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdarg>

namespace LOG
{

/*--------------------------------------------------------------------------*
 * ANSI Color Codes
 *--------------------------------------------------------------------------*/
namespace Color
{
    constexpr const char* RESET     	= "\033[0m";
    constexpr const char* BOLD      	= "\033[1m";
    constexpr const char* DIM       	= "\033[2m";
    constexpr const char* UNDERLINE 	= "\033[4m";
    
    constexpr const char* BLACK     	= "\033[30m";
    constexpr const char* RED       	= "\033[31m";
    constexpr const char* GREEN     	= "\033[32m";
    constexpr const char* YELLOW    	= "\033[93m";
    constexpr const char* BLUE      	= "\033[94m";
    constexpr const char* MAGENTA   	= "\033[95m";
    constexpr const char* CYAN      	= "\033[36m";
    constexpr const char* WHITE     	= "\033[37m";
    
    constexpr const char* BOLD_BLACK    = "\033[1;30m";
    constexpr const char* BOLD_RED      = "\033[1;31m";
    constexpr const char* BOLD_GREEN    = "\033[1;32m";
    constexpr const char* BOLD_YELLOW   = "\033[1;33m";
    constexpr const char* BOLD_BLUE     = "\033[1;94m";
    constexpr const char* BOLD_MAGENTA  = "\033[1;95m";
    constexpr const char* BOLD_CYAN     = "\033[1;36m";
    constexpr const char* BOLD_WHITE    = "\033[1;37m";
    
    constexpr const char* BG_RED        = "\033[41m";
    constexpr const char* BG_GREEN      = "\033[42m";
    constexpr const char* BG_YELLOW     = "\033[43m";
    constexpr const char* BG_BLUE       = "\033[104m";

    constexpr const char* BG_DARK_GREY   = "\033[48;5;236m";
    constexpr const char* BG_DARK_BLUE   = "\033[48;5;17m";
    constexpr const char* BG_DARK_GREEN  = "\033[48;5;22m";
    constexpr const char* BG_DARK_YELLOW = "\033[48;5;58m";
}

/*--------------------------------------------------------------------------*
 * Log Level Enum
 *--------------------------------------------------------------------------*/
enum class LogLevel
{
    TRACE,   // Very detailed debug info
    DEBUG,   // Debug information
    INFO,    // General information
    SUCCESS, // Successful operations
    WARNING, // Warning messages
    ERROR,   // Error messages
    CRITICAL // Critical errors
};

/*--------------------------------------------------------------------------*
 * Box Builder - Flexible box rendering with borders
 *--------------------------------------------------------------------------*/
class BoxBuilder
{
private:
    std::vector<std::string>    lines;
    std::string                 title;
    int                         width;
    bool                        hasTitle;
    
    // Helper function to count visible characters (excluding ANSI codes)
    static size_t countVisibleChars(const std::string& str)
    {
        size_t count = 0;
        bool inEscape = false;
        
        for (size_t i = 0; i < str.length(); ++i)
        {
            if (str[i] == '\033')
            {
                inEscape = true;
            }
            else if (inEscape)
            {
                if (str[i] == 'm')
                {
                    inEscape = false;
                }
            }
            else
            {
                count++;
            }
        }
        
        return count;
    }
    
    // Helper function to pad string to visible width
    static std::string padToWidth(const std::string& str, int targetWidth)
    {
        size_t visibleChars = countVisibleChars(str);
        if (visibleChars >= static_cast<size_t>(targetWidth))
        {
            return str;
        }
        
        size_t padding = targetWidth - visibleChars;
        return str + std::string(padding, ' ');
    }
    
public:
    BoxBuilder(int boxWidth = 65) 
        : width(boxWidth)
        , hasTitle(false) 
    {}
    
    // Set title
    BoxBuilder& setTitle(const std::string& t)
    {
        title = t;
        hasTitle = true;
        return *this;
    }
    
    // Add a line
    BoxBuilder& addLine(const std::string& line)
    {
        lines.push_back(line);
        return *this;
    }
    
    // Add a formatted line
    BoxBuilder& addLinef(const char* format, ...)
    {
        char buffer[512];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        
        lines.push_back(std::string(buffer));
        return *this;
    }
    
    // Add separator
    BoxBuilder& addSeparator()
    {
        lines.push_back("SEPARATOR");
        return *this;
    }
    
    // Render the box
    void render(std::ostream& out = std::cout) const
    {
        // Top border
        out << " " << Color::RESET << "┌";
        for (int i = 0; i < width; i++)
        {
            out << "─";
        }
        out << "┐\n";
        
        // Title
        if (hasTitle == true)
        {
            std::string paddedTitle = padToWidth(title, width - 2);
            out << " " << Color::RESET << "│ " 
                << paddedTitle
                << Color::RESET << " │\n";
            
            // Separator after title
            out << " " << Color::RESET << "├";
            for (int i = 0; i < width; i++)
            {
                out << "─";
            }
            out << "┤\n";
        }
        
        // Lines
        for (const auto& line : lines)
        {
            if (line == "SEPARATOR")
            {
                out << " " << Color::RESET << "├";
                for (int i = 0; i < width; i++)
                {
                    out << "─";
                }
                out << "┤\n";
            }
            else
            {
                std::string paddedLine = padToWidth(line, width - 2);
                out << " " << Color::RESET << "│ " 
                    << paddedLine
                    << Color::RESET << " │\n";
            }
        }
        
        // Bottom border
        out << " " << Color::RESET << "└";
        for (int i = 0; i < width; i++)
        {
            out << "─";
        }
        out << "┘\n";
        
        out << "\n";
    }
    
    // Clear all data
    void clear()
    {
        lines.clear();
        title.clear();
        hasTitle = false;
    }
};

/*--------------------------------------------------------------------------*
 * Table Builder - Flexible table rendering
 *--------------------------------------------------------------------------*/
class TableBuilder
{
private:
    std::vector<std::string>                    headers;
    std::vector<int>                            columnWidths;
    std::vector<std::vector<std::string>>       rows;
    std::string                                 title;
    bool                                        hasTitle;
    
public:
    TableBuilder() : hasTitle(false) {}
    
    // Set title
    TableBuilder& setTitle(const std::string& t)
    {
        title = t;
        hasTitle = true;
        return *this;
    }
    
    // Set headers with automatic width calculation
    TableBuilder& setHeaders(const std::vector<std::string>& h)
    {
        headers = h;
        columnWidths.clear();
        for (const auto& header : headers)
        {
            columnWidths.push_back(static_cast<int>(header.length()));
        }
        return *this;
    }

    TableBuilder& setHeaders(const std::vector<std::string>& h, const std::vector<int>& widths = {})
    {
        headers = h;
        if (widths.empty() == false)
        {
            columnWidths = widths;
        }
        else
        {
            // Auto-calculate widths (minimum 12)
            columnWidths.clear();
            for (const auto& header : headers)
            {
                columnWidths.push_back(std::max(12, static_cast<int>(header.length()) + 2));
            }
        }
        return *this;
    }
    
    // Add a row
    TableBuilder& addRow(const std::vector<std::string>& row)
    {
        rows.push_back(row);
        return *this;
    }
    
    // Add a formatted row
    TableBuilder& addRowf(const char* format, ...)
    {
        char buffer[512];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        
        // Split by spaces or tabs
        std::vector<std::string> row;
        std::istringstream iss(buffer);
        std::string token;
        while (iss >> token)
        {
            row.push_back(token);
        }
        rows.push_back(row);
        return *this;
    }
    
    // Render the table
    void render(std::ostream& out = std::cout) const
    {
        if (headers.empty() == true)
        {
            return;
        }
        
        int totalWidth = 0;
        for (int w : columnWidths)
        {
            totalWidth += w + 1;
        }
        totalWidth -= 1;
        
        // Title
        if (hasTitle == true)
        {
            out << Color::BOLD_CYAN << title << Color::RESET << "\n";
            out << std::string(totalWidth, '=') << "\n";
        }
        
        // Header
        for (size_t i = 0; i < headers.size(); ++i)
        {
            out << std::left << std::setw(columnWidths[i]) << headers[i];
            if (i < headers.size() - 1)
            {
                out << " ";
            }
        }
        out << "\n";
        
        // Separator
        out << std::string(totalWidth, '-') << "\n";
        
        // Rows
        for (const auto& row : rows)
        {
            for (size_t i = 0; i < row.size() && i < headers.size(); ++i)
            {
                out << std::left << std::setw(columnWidths[i]) << row[i];
                if (i < headers.size() - 1)
                {
                    out << " ";
                }
            }
            out << "\n";
        }
        
        out << "\n";
    }
    
    // Clear all data
    void clear()
    {
        headers.clear();
        columnWidths.clear();
        rows.clear();
        title.clear();
        hasTitle = false;
    }
};

/*--------------------------------------------------------------------------*
 * Logger - Static logging interface
 *--------------------------------------------------------------------------*/
class Logger
{
public:
    /*--------------------------------------------------------------------------*
     * Core logging function
     *--------------------------------------------------------------------------*/
    static void log(const char *module, LogLevel level, const char *message)
    {
        const char* levelStr = "INFO";
        const char* levelColor = Color::WHITE;

        switch (level)
        {
            case LogLevel::TRACE:
                levelStr = "TRACE";
                levelColor = Color::DIM;
                break;
            case LogLevel::DEBUG:
                levelStr = "DEBUG";
                levelColor = Color::CYAN;
                break;
            case LogLevel::INFO:
                levelStr = "INFO";
                levelColor = Color::WHITE;
                break;
            case LogLevel::SUCCESS:
                levelStr = "OK";
                levelColor = Color::BOLD_GREEN;
                break;
            case LogLevel::WARNING:
                levelStr = "WARN";
                levelColor = Color::YELLOW;
                break;
            case LogLevel::ERROR:
                levelStr = "ERROR";
                levelColor = Color::BOLD_RED;
                break;
            case LogLevel::CRITICAL:
                levelStr = "CRITICAL";
                levelColor = Color::BG_RED;
                break;
        }

        std::printf("%s[%s]%s %-8s %s\n",
                    Color::BOLD_WHITE, module, Color::RESET,
                    (std::string(levelColor) + levelStr + Color::RESET).c_str(),
                    message);
    }

    /*--------------------------------------------------------------------------*
     * Formatted logging
     *--------------------------------------------------------------------------*/
    static void logf(const char *module, LogLevel level, const char *format, ...)
    {
        char buffer[512];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        log(module, level, buffer);
    }

    /*--------------------------------------------------------------------------*
     * Convenience functions (require module name)
     *--------------------------------------------------------------------------*/
    static void trace(const char *module, const char *msg) 		{ log(module, LogLevel::TRACE, msg); }
    static void debug(const char *module, const char *msg) 		{ log(module, LogLevel::DEBUG, msg); }
    static void info(const char *module, const char *msg) 		{ log(module, LogLevel::INFO, msg); }
    static void success(const char *module, const char *msg) 	{ log(module, LogLevel::SUCCESS, msg); }
    static void warning(const char *module, const char *msg) 	{ log(module, LogLevel::WARNING, msg); }
    static void error(const char *module, const char *msg) 		{ log(module, LogLevel::ERROR, msg); }
    static void critical(const char *module, const char *msg)	{ log(module, LogLevel::CRITICAL, msg); }

    /*--------------------------------------------------------------------------*
     * Section/Alert headers
     *--------------------------------------------------------------------------*/
    static void section(const char *module, const char *title)
    {
        std::printf("\n%s[%s]%s %s=== %s ===%s\n",
                    Color::BOLD_WHITE, module, Color::RESET,
                    Color::BOLD_CYAN, title, Color::RESET);
    }

    static void alert(const char *module, const char *title)
    {
        std::printf("\n%s[%s]%s %s=== %s ===%s\n",
                    Color::BOLD_WHITE, module, Color::RESET,
                    Color::BOLD_RED, title, Color::RESET);
    }

    /*--------------------------------------------------------------------------*
     * Pipeline strings
     *--------------------------------------------------------------------------*/
    static void pipeline(const char *module, const char *pipeName, const char *pipeStr)
    {
        std::printf("\n%s[%s]%s %s=== %s Pipeline ===%s\n%s%s%s\n",
                    Color::BOLD_WHITE, module, Color::RESET,
                    Color::BOLD_CYAN, pipeName, Color::RESET,
                    Color::GREEN, pipeStr, Color::RESET);
    }

    /*--------------------------------------------------------------------------*
     * Builder Interface
     *--------------------------------------------------------------------------*/
    static TableBuilder createTable()
    {
        return TableBuilder();
    }
    
    static BoxBuilder createBox(int width = 65)
    {
        return BoxBuilder(width);
    }

    /*--------------------------------------------------------------------------*
     * Raw colored output
     *--------------------------------------------------------------------------*/
    static void raw(const char *color, const char *message)
    {
        std::printf("%s%s%s\n", color, message, Color::RESET);
    }

    static void rawf(const char *color, const char *format, ...)
    {
        char buffer[512];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        std::printf("%s%s%s\n", color, buffer, Color::RESET);
    }
};

} // namespace LOG

/*--------------------------------------------------------------------------*
 * Module-Specific Wrappers
 *--------------------------------------------------------------------------*/

// DVR Module
#define LOG_DVR_TRACE(msg)      	LOG::Logger::trace("DVR", msg)
#define LOG_DVR_DEBUG(msg)      	LOG::Logger::debug("DVR", msg)
#define LOG_DVR_INFO(msg)       	LOG::Logger::info("DVR", msg)
#define LOG_DVR_SUCCESS(msg)    	LOG::Logger::success("DVR", msg)
#define LOG_DVR_WARNING(msg)    	LOG::Logger::warning("DVR", msg)
#define LOG_DVR_ERROR(msg)      	LOG::Logger::error("DVR", msg)
#define LOG_DVR_CRITICAL(msg)   	LOG::Logger::critical("DVR", msg)

#define LOG_DVR_TRACEF(fmt, ...)    LOG::Logger::logf("DVR", LOG::LogLevel::TRACE, fmt, ##__VA_ARGS__)
#define LOG_DVR_DEBUGF(fmt, ...)    LOG::Logger::logf("DVR", LOG::LogLevel::DEBUG, fmt, ##__VA_ARGS__)
#define LOG_DVR_INFOF(fmt, ...)     LOG::Logger::logf("DVR", LOG::LogLevel::INFO, fmt, ##__VA_ARGS__)
#define LOG_DVR_SUCCESSF(fmt, ...)  LOG::Logger::logf("DVR", LOG::LogLevel::SUCCESS, fmt, ##__VA_ARGS__)
#define LOG_DVR_WARNINGF(fmt, ...)  LOG::Logger::logf("DVR", LOG::LogLevel::WARNING, fmt, ##__VA_ARGS__)
#define LOG_DVR_ERRORF(fmt, ...)    LOG::Logger::logf("DVR", LOG::LogLevel::ERROR, fmt, ##__VA_ARGS__)

#define LOG_DVR_SECTION(title) 		LOG::Logger::section("DVR", title)
#define LOG_DVR_ALERT(title) 		LOG::Logger::alert("DVR", title)
#define LOG_DVR_PIPELINE(name, str) LOG::Logger::pipeline("DVR", name, str)

// VPB Module
#define LOG_VPB_TRACE(msg)      	LOG::Logger::trace("VPB", msg)
#define LOG_VPB_DEBUG(msg)      	LOG::Logger::debug("VPB", msg)
#define LOG_VPB_INFO(msg)       	LOG::Logger::info("VPB", msg)
#define LOG_VPB_SUCCESS(msg)    	LOG::Logger::success("VPB", msg)
#define LOG_VPB_WARNING(msg)    	LOG::Logger::warning("VPB", msg)
#define LOG_VPB_ERROR(msg)      	LOG::Logger::error("VPB", msg)
#define LOG_VPB_CRITICAL(msg)   	LOG::Logger::critical("VPB", msg)

#define LOG_VPB_TRACEF(fmt, ...)    LOG::Logger::logf("VPB", LOG::LogLevel::TRACE, fmt, ##__VA_ARGS__)
#define LOG_VPB_DEBUGF(fmt, ...)    LOG::Logger::logf("VPB", LOG::LogLevel::DEBUG, fmt, ##__VA_ARGS__)
#define LOG_VPB_INFOF(fmt, ...)     LOG::Logger::logf("VPB", LOG::LogLevel::INFO, fmt, ##__VA_ARGS__)
#define LOG_VPB_SUCCESSF(fmt, ...)  LOG::Logger::logf("VPB", LOG::LogLevel::SUCCESS, fmt, ##__VA_ARGS__)
#define LOG_VPB_WARNINGF(fmt, ...)  LOG::Logger::logf("VPB", LOG::LogLevel::WARNING, fmt, ##__VA_ARGS__)
#define LOG_VPB_ERRORF(fmt, ...)    LOG::Logger::logf("VPB", LOG::LogLevel::ERROR, fmt, ##__VA_ARGS__)

#define LOG_VPB_SECTION(title) 		LOG::Logger::section("VPB", title)
#define LOG_VPB_ALERT(title) 		LOG::Logger::alert("VPB", title)
#define LOG_VPB_PIPELINE(name, str) LOG::Logger::pipeline("VPB", name, str)

// RKNN Module
#define LOG_RKNN_TRACE(msg)         LOG::Logger::trace("RKNN", msg)
#define LOG_RKNN_DEBUG(msg)         LOG::Logger::debug("RKNN", msg)
#define LOG_RKNN_INFO(msg)          LOG::Logger::info("RKNN", msg)
#define LOG_RKNN_SUCCESS(msg)       LOG::Logger::success("RKNN", msg)
#define LOG_RKNN_WARNING(msg)       LOG::Logger::warning("RKNN", msg)
#define LOG_RKNN_ERROR(msg)         LOG::Logger::error("RKNN", msg)

#define LOG_RKNN_TRACEF(fmt, ...)   LOG::Logger::logf("RKNN", LOG::LogLevel::TRACE, fmt, ##__VA_ARGS__)
#define LOG_RKNN_DEBUGF(fmt, ...)   LOG::Logger::logf("RKNN", LOG::LogLevel::DEBUG, fmt, ##__VA_ARGS__)
#define LOG_RKNN_INFOF(fmt, ...)    LOG::Logger::logf("RKNN", LOG::LogLevel::INFO, fmt, ##__VA_ARGS__)
#define LOG_RKNN_SUCCESSF(fmt, ...) LOG::Logger::logf("RKNN", LOG::LogLevel::SUCCESS, fmt, ##__VA_ARGS__)
#define LOG_RKNN_WARNINGF(fmt, ...) LOG::Logger::logf("RKNN", LOG::LogLevel::WARNING, fmt, ##__VA_ARGS__)
#define LOG_RKNN_ERRORF(fmt, ...)   LOG::Logger::logf("RKNN", LOG::LogLevel::ERROR, fmt, ##__VA_ARGS__)

#define LOG_RKNN_SECTION(title)     LOG::Logger::section("RKNN", title)
#define LOG_RKNN_ALERT(title) 		LOG::Logger::alert("RKNN", title)

// GST Module (GStreamer pipeline)
#define LOG_GST_TRACE(msg)          LOG::Logger::trace("GST", msg)
#define LOG_GST_DEBUG(msg)          LOG::Logger::debug("GST", msg)
#define LOG_GST_INFO(msg)           LOG::Logger::info("GST", msg)
#define LOG_GST_SUCCESS(msg)        LOG::Logger::success("GST", msg)
#define LOG_GST_WARNING(msg)        LOG::Logger::warning("GST", msg)
#define LOG_GST_ERROR(msg)          LOG::Logger::error("GST", msg)

#define LOG_GST_TRACEF(fmt, ...)    LOG::Logger::logf("GST", LOG::LogLevel::TRACE, fmt, ##__VA_ARGS__)
#define LOG_GST_DEBUGF(fmt, ...)    LOG::Logger::logf("GST", LOG::LogLevel::DEBUG, fmt, ##__VA_ARGS__)
#define LOG_GST_INFOF(fmt, ...)     LOG::Logger::logf("GST", LOG::LogLevel::INFO, fmt, ##__VA_ARGS__)
#define LOG_GST_SUCCESSF(fmt, ...)  LOG::Logger::logf("GST", LOG::LogLevel::SUCCESS, fmt, ##__VA_ARGS__)
#define LOG_GST_WARNINGF(fmt, ...)  LOG::Logger::logf("GST", LOG::LogLevel::WARNING, fmt, ##__VA_ARGS__)
#define LOG_GST_ERRORF(fmt, ...)    LOG::Logger::logf("GST", LOG::LogLevel::ERROR, fmt, ##__VA_ARGS__)

#define LOG_GST_SECTION(title) 		LOG::Logger::section("GST", title)
#define LOG_GST_ALERT(title) 		LOG::Logger::alert("GST", title)
#define LOG_GST_PIPELINE(name, str) LOG::Logger::pipeline("GST", name, str)

// APP Module (Application)
#define LOG_APP_TRACE(msg)          LOG::Logger::trace("APP", msg)
#define LOG_APP_DEBUG(msg)          LOG::Logger::debug("APP", msg)
#define LOG_APP_INFO(msg)           LOG::Logger::info("APP", msg)
#define LOG_APP_SUCCESS(msg)        LOG::Logger::success("APP", msg)
#define LOG_APP_WARNING(msg)        LOG::Logger::warning("APP", msg)
#define LOG_APP_ERROR(msg)          LOG::Logger::error("APP", msg)
#define LOG_APP_CRITICAL(msg)       LOG::Logger::critical("APP", msg)

#define LOG_APP_TRACEF(fmt, ...)    LOG::Logger::logf("APP", LOG::LogLevel::TRACE, fmt, ##__VA_ARGS__)
#define LOG_APP_DEBUGF(fmt, ...)    LOG::Logger::logf("APP", LOG::LogLevel::DEBUG, fmt, ##__VA_ARGS__)
#define LOG_APP_INFOF(fmt, ...)     LOG::Logger::logf("APP", LOG::LogLevel::INFO, fmt, ##__VA_ARGS__)
#define LOG_APP_SUCCESSF(fmt, ...)  LOG::Logger::logf("APP", LOG::LogLevel::SUCCESS, fmt, ##__VA_ARGS__)
#define LOG_APP_WARNINGF(fmt, ...)  LOG::Logger::logf("APP", LOG::LogLevel::WARNING, fmt, ##__VA_ARGS__)
#define LOG_APP_ERRORF(fmt, ...)    LOG::Logger::logf("APP", LOG::LogLevel::ERROR, fmt, ##__VA_ARGS__)

#define LOG_APP_SECTION(title)      LOG::Logger::section("APP", title)
#define LOG_APP_ALERT(title) 		LOG::Logger::alert("APP", title)

// UI Module (User Interface)
#define LOG_UI_TRACE(msg)           LOG::Logger::trace("UI", msg)
#define LOG_UI_DEBUG(msg)           LOG::Logger::debug("UI", msg)
#define LOG_UI_INFO(msg)            LOG::Logger::info("UI", msg)
#define LOG_UI_SUCCESS(msg)         LOG::Logger::success("UI", msg)
#define LOG_UI_WARNING(msg)         LOG::Logger::warning("UI", msg)
#define LOG_UI_ERROR(msg)           LOG::Logger::error("UI", msg)

#define LOG_UI_TRACEF(fmt, ...)     LOG::Logger::logf("UI", LOG::LogLevel::TRACE, fmt, ##__VA_ARGS__)
#define LOG_UI_DEBUGF(fmt, ...)     LOG::Logger::logf("UI", LOG::LogLevel::DEBUG, fmt, ##__VA_ARGS__)
#define LOG_UI_INFOF(fmt, ...)      LOG::Logger::logf("UI", LOG::LogLevel::INFO, fmt, ##__VA_ARGS__)
#define LOG_UI_SUCCESSF(fmt, ...)   LOG::Logger::logf("UI", LOG::LogLevel::SUCCESS, fmt, ##__VA_ARGS__)
#define LOG_UI_WARNINGF(fmt, ...)   LOG::Logger::logf("UI", LOG::LogLevel::WARNING, fmt, ##__VA_ARGS__)
#define LOG_UI_ERRORF(fmt, ...)     LOG::Logger::logf("UI", LOG::LogLevel::ERROR, fmt, ##__VA_ARGS__)

#define LOG_UI_SECTION(title)       LOG::Logger::section("UI", title)
#define LOG_UI_ALERT(title) 		LOG::Logger::alert("UI", title)

// IO Module (Input/Output)
#define LOG_IO_TRACE(msg)           LOG::Logger::trace("IO", msg)
#define LOG_IO_DEBUG(msg)           LOG::Logger::debug("IO", msg)
#define LOG_IO_INFO(msg)            LOG::Logger::info("IO", msg)
#define LOG_IO_SUCCESS(msg)         LOG::Logger::success("IO", msg)
#define LOG_IO_WARNING(msg)         LOG::Logger::warning("IO", msg)
#define LOG_IO_ERROR(msg)           LOG::Logger::error("IO", msg)

#define LOG_IO_TRACEF(fmt, ...)     LOG::Logger::logf("IO", LOG::LogLevel::TRACE, fmt, ##__VA_ARGS__)
#define LOG_IO_DEBUGF(fmt, ...)     LOG::Logger::logf("IO", LOG::LogLevel::DEBUG, fmt, ##__VA_ARGS__)
#define LOG_IO_INFOF(fmt, ...)      LOG::Logger::logf("IO", LOG::LogLevel::INFO, fmt, ##__VA_ARGS__)
#define LOG_IO_SUCCESSF(fmt, ...)   LOG::Logger::logf("IO", LOG::LogLevel::SUCCESS, fmt, ##__VA_ARGS__)
#define LOG_IO_WARNINGF(fmt, ...)   LOG::Logger::logf("IO", LOG::LogLevel::WARNING, fmt, ##__VA_ARGS__)
#define LOG_IO_ERRORF(fmt, ...)     LOG::Logger::logf("IO", LOG::LogLevel::ERROR, fmt, ##__VA_ARGS__)

#define LOG_IO_SECTION(title)       LOG::Logger::section("IO", title)
#define LOG_IO_ALERT(title) 		LOG::Logger::alert("IO", title)

#endif // __LOGGER_H__
