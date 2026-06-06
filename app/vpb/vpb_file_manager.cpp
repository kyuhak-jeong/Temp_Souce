#include "vpb_file_manager.h"
#include <algorithm>
#include <sys/stat.h>
#include <iostream>

namespace VPB
{

// ============================================================================
// Constructor / Destructor
// ============================================================================

FileManager::FileManager() 
    : m_rootDirectory("")
    , m_currentDirectory(".")
    , m_files()
    , m_groups()
    , m_extensionFilter()
{
}

FileManager::~FileManager()
{
}

// ============================================================================
// Directory operations
// ============================================================================

bool FileManager::setCurrentDirectory(const std::string& path)
{
    if (FileUtils::isDirectory(path) == false) return false;
    m_currentDirectory = path;
    return true;
}

bool FileManager::navigateToParent()
{
    std::string parent = FileUtils::getParentDirectory(m_currentDirectory);
    if (parent == m_currentDirectory) return false;
    if (BLOCK_FILE_NAV == true && m_rootDirectory.empty() == false && parent.rfind(m_rootDirectory, 0) != 0)
        return false;
    return setCurrentDirectory(parent);
}

bool FileManager::navigateToChild(const std::string& subdir)
{
    std::string childPath = FileUtils::joinPath(m_currentDirectory, subdir);
    return setCurrentDirectory(childPath);
}

// ============================================================================
// File list operations
// ============================================================================

bool FileManager::scanDirectory()
{
    clearFileList();
    
    std::vector<std::string> entries = FileUtils::listDirectory(m_currentDirectory);
    
    // Process all directory entries
    for (const std::string& entry : entries)
    {
        std::string fullPath = FileUtils::joinPath(m_currentDirectory, entry);
        bool isDir = FileUtils::isDirectory(fullPath);
        
        // Skip files that don't match filter (but always include directories)
        if (isDir == false)
        {
            std::string extension = FileUtils::getFileExtension(entry);
            if (matchesFilter(extension) == false) continue;
        }
        
        FileInfo info;
        info.path        = fullPath;
        info.name        = entry;
        info.extension   = isDir ? "" : FileUtils::getFileExtension(entry);
        info.isDirectory = isDir;
        
        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0)
        {
            info.size         = st.st_size;
            info.modifiedTime = st.st_mtime;
        }
        
        m_files.push_back(info);
    }
    
    sortFilesByName();
    updateFileIndices();
    return true;
}

void FileManager::clearFileList()
{
    m_files.clear();
}

FileInfo* FileManager::getFileAt(int32_t index)
{
    if (index >= 0 && index < static_cast<int32_t>(m_files.size())) return &m_files[index];
    return nullptr;
}

FileInfo* FileManager::findFileByName(const std::string& name)
{
    for (auto& file : m_files)
    {
        if (file.name == name) return &file;
    }
    return nullptr;
}

// ============================================================================
// Group operations
// ============================================================================

void FileManager::createGroup(const std::string& name, int32_t startIndex, int32_t count)
{
    if (startIndex < 0 || startIndex >= static_cast<int32_t>(m_files.size())) return;
    
    if (count <= 0 || startIndex + count > static_cast<int32_t>(m_files.size()))
        count = static_cast<int32_t>(m_files.size()) - startIndex;
    
    GroupInfo group;
    group.name       = name;
    group.startIndex = startIndex;
    group.count      = count;
    group.totalSize  = 0;
    
    for (int32_t i = startIndex; i < startIndex + count; ++i)
    {
        m_files[i].groupIndex   = static_cast<int32_t>(m_groups.size());
        m_files[i].indexInGroup = i - startIndex;
        group.totalSize        += m_files[i].size;
    }
    
    m_groups.push_back(group);
}

void FileManager::clearGroups()
{
    m_groups.clear();
    for (auto& file : m_files)
    {
        file.groupIndex   = -1;
        file.indexInGroup = -1;
    }
}

GroupInfo* FileManager::getGroupAt(int32_t index)
{
    if (index >= 0 && index < static_cast<int32_t>(m_groups.size())) return &m_groups[index];
    return nullptr;
}

GroupInfo* FileManager::findGroupByName(const std::string& name)
{
    for (auto& group : m_groups)
    {
        if (group.name == name) return &group;
    }
    return nullptr;
}

GroupInfo* FileManager::findGroupContaining(int32_t fileIndex)
{
    if (fileIndex < 0 || fileIndex >= static_cast<int32_t>(m_files.size())) return nullptr;
    
    int32_t groupIndex = m_files[fileIndex].groupIndex;
    if (groupIndex < 0 || groupIndex >= static_cast<int32_t>(m_groups.size())) return nullptr;
    return &m_groups[groupIndex];
}

// ============================================================================
// File operations
// ============================================================================

bool FileManager::copyFile(int32_t index, const std::string& dstPath)
{
    FileInfo* file = getFileAt(index);
    if (file == nullptr || file->isDirectory == true) return false;
    
    std::string dst = FileUtils::joinPath(dstPath, file->name);
    return FileUtils::copyFile(file->path, dst);
}

bool FileManager::deleteFile(int32_t index)
{
    FileInfo* file = getFileAt(index);
    if (file == nullptr || file->isDirectory == true) return false;
    
    if (FileUtils::deleteFile(file->path) == false) return false;
    
    m_files.erase(m_files.begin() + index);
    updateFileIndices();
    return true;
}

bool FileManager::copyGroup(int32_t groupIndex, const std::string& dstPath)
{
    GroupInfo* group = getGroupAt(groupIndex);
    if (group == nullptr) return false;
    
    for (int32_t i = group->startIndex; i < group->startIndex + group->count; ++i)
    {
        if (copyFile(i, dstPath) == false) return false;
    }
    return true;
}

bool FileManager::deleteGroup(int32_t groupIndex)
{
    GroupInfo* group = getGroupAt(groupIndex);
    if (group == nullptr) return false;
    
    for (int32_t i = group->startIndex + group->count - 1; i >= group->startIndex; --i)
    {
        if (deleteFile(i) == false) return false;
    }
    
    m_groups.erase(m_groups.begin() + groupIndex);
    return true;
}

// ============================================================================
// Navigation
// ============================================================================

int32_t FileManager::getNextFileIndex(int32_t currentIndex) const
{
    if (m_files.empty() == true) return -1;
    return (currentIndex + 1) % static_cast<int32_t>(m_files.size());
}

int32_t FileManager::getPrevFileIndex(int32_t currentIndex) const
{
    if (m_files.empty() == true) return -1;
    if (currentIndex <= 0) return static_cast<int32_t>(m_files.size()) - 1;
    return currentIndex - 1;
}

int32_t FileManager::getNextGroupIndex(int32_t currentIndex) const
{
    if (m_files.empty() == true) return -1;
    
    GroupInfo* currentGroup = const_cast<FileManager*>(this)->findGroupContaining(currentIndex);
    if (currentGroup == nullptr) return getNextFileIndex(currentIndex);
    
    int32_t nextIndex = currentGroup->getEndIndex() + 1;
    if (nextIndex >= static_cast<int32_t>(m_files.size())) nextIndex = 0;
    return nextIndex;
}

int32_t FileManager::getPrevGroupIndex(int32_t currentIndex) const
{
    if (m_files.empty() == true) return -1;
    
    GroupInfo* currentGroup = const_cast<FileManager*>(this)->findGroupContaining(currentIndex);
    if (currentGroup == nullptr) return getPrevFileIndex(currentIndex);
    
    int32_t prevIndex = currentGroup->startIndex - 1;
    if (prevIndex < 0) prevIndex = static_cast<int32_t>(m_files.size()) - 1;
    return prevIndex;
}

// ============================================================================
// Filters
// ============================================================================

void FileManager::setExtensionFilter(const std::vector<std::string>& extensions)
{
    m_extensionFilter.clear();
    for (const auto& ext : extensions)
    {
        std::string lowerExt = ext;
        std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);
        m_extensionFilter.push_back(lowerExt);
    }
}

void FileManager::clearExtensionFilter()
{
    m_extensionFilter.clear();
}

// ============================================================================
// Private methods
// ============================================================================

bool FileManager::matchesFilter(const std::string& extension) const
{
    if (m_extensionFilter.empty() == true) return true;
    
    std::string lowerExt = extension;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);
    
    for (const std::string& ext : m_extensionFilter)
    {
        if (lowerExt == ext) return true;
    }
    return false;
}

void FileManager::updateFileIndices()
{
    for (int32_t i = 0; i < static_cast<int32_t>(m_files.size()); ++i)
        m_files[i].globalIndex = i;
}

void FileManager::sortFilesByName()
{
    // Separate directories and files
    std::vector<FileInfo> directories;
    std::vector<FileInfo> files;
    
    for (auto& file : m_files)
    {
        if (file.isDirectory == true)
            directories.push_back(file);
        else
            files.push_back(file);
    }
    
    // Sort directories (.. always first, then alphabetically)
    std::sort(directories.begin(), directories.end(), 
        [](const FileInfo& a, const FileInfo& b) 
        {
            if (a.isParentDir() == true) return true;
            if (b.isParentDir() == true) return false;
            return a.name < b.name;
        });
    
    // Sort files alphabetically
    std::sort(files.begin(), files.end(), 
        [](const FileInfo& a, const FileInfo& b) { return a.name > b.name; });
    
    // Merge: directories first, then files
    m_files.clear();
    m_files.insert(m_files.end(), directories.begin(), directories.end());
    m_files.insert(m_files.end(), files.begin(), files.end());
}

} // namespace VPB
