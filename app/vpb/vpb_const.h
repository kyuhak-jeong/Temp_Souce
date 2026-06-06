#ifndef VPB_CONST_H
#define VPB_CONST_H

#include <cstdint>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>

namespace VPB
{
constexpr bool BLOCK_FILE_NAV = true;

enum class PlaybackState { STOPPED, PLAYING, PAUSED, ERROR };
enum class Layout { SINGLE, GRID };
enum class PipelineMode { CPU, GPU };
enum class FileSortMode { NAME_ASC, NAME_DESC, DATE_ASC, DATE_DESC, SIZE_ASC, SIZE_DESC };

struct FileMetadata
{
    bool    isValid;
    int64_t duration;
    int32_t videoTrackCount, audioTrackCount, subtitleTrackCount;
    int32_t width, height;
    float   framerate;
    
    FileMetadata() : isValid(false), duration(0), videoTrackCount(0), audioTrackCount(0),
                     subtitleTrackCount(0), width(0), height(0), framerate(0.0f) {}
};

struct FileInfo
{
    std::string  path, name, extension;
    int64_t      size, modifiedTime;
    int32_t      groupIndex, indexInGroup, globalIndex;
    bool         isDirectory;
    FileMetadata metadata;
    
    FileInfo() : size(0), modifiedTime(0), groupIndex(-1), indexInGroup(-1), 
                 globalIndex(-1), isDirectory(false) {}
    
    bool isParentDir() const { return name == ".."; }
    bool isCurrentDir() const { return name == "."; }
};

struct GroupInfo
{
    std::string name;
    int32_t     startIndex, count;
    int64_t     totalSize;
    bool        isDirectory;
    
    GroupInfo() : startIndex(0), count(0), totalSize(0), isDirectory(false) {}
    
    int32_t getEndIndex() const { return startIndex + count - 1; }
    bool contains(int32_t index) const { return index >= startIndex && index < startIndex + count; }
};

// ============================================================================
// FileUtils
// ============================================================================

namespace FileUtils
{
    inline bool fileExists(const std::string& path)
    {
        struct stat buffer;
        return (stat(path.c_str(), &buffer) == 0);
    }

    inline bool isDirectory(const std::string& path)
    {
        struct stat buffer;
        if (stat(path.c_str(), &buffer) != 0) return false;
        return S_ISDIR(buffer.st_mode);
    }

    inline std::string getFileName(const std::string& path)
    {
        size_t pos = path.find_last_of("/\\");
        if (pos == std::string::npos) return path;
        return path.substr(pos + 1);
    }

    inline std::string getFileExtension(const std::string& path)
    {
        std::string filename = getFileName(path);
        size_t pos = filename.find_last_of('.');
        if (pos == std::string::npos) return "";
        std::string ext = filename.substr(pos + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext;
    }

    inline std::string getCurrentWorkingDirectory()
    {
        char buffer[1024];
        if (getcwd(buffer, sizeof(buffer)) != nullptr) return std::string(buffer);
        return ".";
    }

    inline std::string getParentDirectory(const std::string& path)
    {
        if (path.empty() == true || path == "/") return path;
        size_t pos = path.find_last_of("/\\");
        if (pos == std::string::npos) return ".";
        if (pos == 0) return "/";
        return path.substr(0, pos);
    }

    inline std::string joinPath(const std::string& dir, const std::string& file)
    {
        if (dir.empty() == true) return file;
        if (dir.back() == '/' || dir.back() == '\\') return dir + file;
        return dir + "/" + file;
    }

    inline bool createDirectory(const std::string& path) { return mkdir(path.c_str(), 0755) == 0; }
    inline bool deleteFile(const std::string& path) { return unlink(path.c_str()) == 0; }

    inline bool copyFile(const std::string& src, const std::string& dst)
    {
        FILE* srcFile = fopen(src.c_str(), "rb");
        if (srcFile == nullptr) return false;
        
        FILE* dstFile = fopen(dst.c_str(), "wb");
        if (dstFile == nullptr)
        {
            fclose(srcFile);
            return false;
        }
        
        char buffer[4096];
        size_t bytesRead = 0;
        
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), srcFile)) > 0)
        {
            if (fwrite(buffer, 1, bytesRead, dstFile) != bytesRead)
            {
                fclose(srcFile);
                fclose(dstFile);
                return false;
            }
        }
        
        fclose(srcFile);
        fclose(dstFile);
        return true;
    }

    inline std::vector<std::string> listDirectory(const std::string& path)
    {
        std::vector<std::string> files;
        DIR* dir = opendir(path.c_str());
        if (dir == nullptr) return files;
        
        struct dirent* entry = nullptr;
        while ((entry = readdir(dir)) != nullptr)
        {
            std::string name = entry->d_name;
            if(name == ".") continue;
            if(name.empty() == false && name[0] == '.' && name != "..") continue;
            files.push_back(name);
        }
        
        closedir(dir);
        return files;
    }

    inline void sortFiles(std::vector<FileInfo>& files, FileSortMode mode)
    {
        switch (mode)
        {
            case FileSortMode::NAME_ASC:
                std::sort(files.begin(), files.end(), [](const FileInfo& a, const FileInfo& b) { return a.name < b.name; });
                break;
            case FileSortMode::NAME_DESC:
                std::sort(files.begin(), files.end(), [](const FileInfo& a, const FileInfo& b) { return a.name > b.name; });
                break;
            case FileSortMode::DATE_ASC:
                std::sort(files.begin(), files.end(), [](const FileInfo& a, const FileInfo& b) { return a.modifiedTime < b.modifiedTime; });
                break;
            case FileSortMode::DATE_DESC:
                std::sort(files.begin(), files.end(), [](const FileInfo& a, const FileInfo& b) { return a.modifiedTime > b.modifiedTime; });
                break;
            case FileSortMode::SIZE_ASC:
                std::sort(files.begin(), files.end(), [](const FileInfo& a, const FileInfo& b) { return a.size < b.size; });
                break;
            case FileSortMode::SIZE_DESC:
                std::sort(files.begin(), files.end(), [](const FileInfo& a, const FileInfo& b) { return a.size > b.size; });
                break;
        }
    }

    inline std::string formatFileSize(int64_t bytes)
    {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unitIndex = 0;
        double size = bytes;
        
        while (size >= 1024.0 && unitIndex < 4)
        {
            size /= 1024.0;
            unitIndex++;
        }
        
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
        return oss.str();
    }
}

// ============================================================================
// TimeUtils
// ============================================================================

namespace TimeUtils
{
    inline std::string formatTime(int64_t milliseconds)
    {
        int64_t totalSeconds = milliseconds / 1000;
        int64_t hours        = totalSeconds / 3600;
        int64_t minutes      = (totalSeconds % 3600) / 60;
        int64_t seconds      = totalSeconds % 60;
        int64_t ms           = milliseconds % 1000;
        
        std::ostringstream oss;
        oss << std::setfill('0') 
            << std::setw(2) << hours << ":"
            << std::setw(2) << minutes << ":"
            << std::setw(2) << seconds << "."
            << std::setw(3) << ms;
        return oss.str();
    }

    inline std::string formatDuration(int64_t milliseconds)
    {
        int64_t totalSeconds = milliseconds / 1000;
        int64_t hours        = totalSeconds / 3600;
        int64_t minutes      = (totalSeconds % 3600) / 60;
        int64_t seconds      = totalSeconds % 60;
        
        std::ostringstream oss;
        if(hours > 0) 
            oss << hours << "h " << minutes << "m " << seconds << "s";
        else if(minutes > 0) 
            oss << minutes << "m " << seconds << "s";
        else 
            oss << seconds << "s";
        return oss.str();
    }

    inline std::string formatDateTime(int64_t timestamp)
    {
        time_t      t       = timestamp;
        struct tm*  tm_info = localtime(&t);
        char        buffer[64];
        
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
        return std::string(buffer);
    }
}

} // namespace VPB

#endif // VPB_CONST_H
