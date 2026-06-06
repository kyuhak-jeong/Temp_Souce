#ifndef VPB_FILE_MANAGER_H
#define VPB_FILE_MANAGER_H

#include "vpb_const.h"
#include <string>
#include <vector>
#include <memory>

namespace VPB
{

class FileManager
{
public:
    FileManager();
    ~FileManager();
    
    // Directory operations
    void        setRootDirectory(const std::string& path)  { m_rootDirectory = path; }
    std::string getRootDirectory()  const                  { return m_rootDirectory; }
    bool        setCurrentDirectory(const std::string& path);
    bool        navigateToParent();
    bool        navigateToChild(const std::string& subdir);
    std::string getCurrentDirectory() const                { return m_currentDirectory; }
    
    // File list operations
    bool      scanDirectory();
    void      clearFileList();
    int32_t   getFileCount() const { return static_cast<int32_t>(m_files.size()); }
    FileInfo* getFileAt(int32_t index);
    FileInfo* findFileByName(const std::string& name);
    
    // Group operations
    void       createGroup(const std::string& name, int32_t startIndex, int32_t count);
    void       clearGroups();
    int32_t    getGroupCount() const { return static_cast<int32_t>(m_groups.size()); }
    GroupInfo* getGroupAt(int32_t index);
    GroupInfo* findGroupByName(const std::string& name);
    GroupInfo* findGroupContaining(int32_t fileIndex);
    
    // File operations
    bool copyFile(int32_t index, const std::string& dstPath);
    bool deleteFile(int32_t index);
    bool copyGroup(int32_t groupIndex, const std::string& dstPath);
    bool deleteGroup(int32_t groupIndex);
    
    // Navigation
    int32_t getNextFileIndex(int32_t currentIndex) const;
    int32_t getPrevFileIndex(int32_t currentIndex) const;
    int32_t getNextGroupIndex(int32_t currentIndex) const;
    int32_t getPrevGroupIndex(int32_t currentIndex) const;
    
    // Filters
    void setExtensionFilter(const std::vector<std::string>& extensions);
    void clearExtensionFilter();
    
private:
    std::string              m_rootDirectory;
    std::string              m_currentDirectory;
    std::vector<FileInfo>    m_files;
    std::vector<GroupInfo>   m_groups;
    std::vector<std::string> m_extensionFilter;
    
    bool matchesFilter(const std::string& extension) const;
    void updateFileIndices();
    void sortFilesByName();
};

} // namespace VPB

#endif // VPB_FILE_MANAGER_H