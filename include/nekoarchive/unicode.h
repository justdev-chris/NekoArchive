#pragma once
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>

// Convert UTF-8 string to wide string (Windows UTF-16)
inline std::wstring utf8_to_wstring(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

// Convert wide string to UTF-8 string
inline std::string wstring_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], size_needed, NULL, NULL);
    return str;
}

// Get list of all files in a directory recursively (handles Unicode)
inline std::vector<std::string> list_files_recursive(const std::string& directory) {
    std::vector<std::string> files;
    std::wstring wdir = utf8_to_wstring(directory);
    std::wstring search_path = wdir + L"\\*";
    
    WIN32_FIND_DATAW find_data;
    HANDLE hFind = FindFirstFileW(search_path.c_str(), &find_data);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return files;
    }
    
    do {
        // Skip . and ..
        if (wcscmp(find_data.cFileName, L".") == 0 || 
            wcscmp(find_data.cFileName, L"..") == 0) {
            continue;
        }
        
        std::wstring full_path = wdir + L"\\" + find_data.cFileName;
        std::string utf8_path = wstring_to_utf8(full_path);
        
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Recursively scan subdirectories
            auto sub_files = list_files_recursive(utf8_path);
            files.insert(files.end(), sub_files.begin(), sub_files.end());
        } else {
            files.push_back(utf8_path);
        }
    } while (FindNextFileW(hFind, &find_data) != 0);
    
    FindClose(hFind);
    return files;
}

// Check if a path is a directory (handles Unicode)
inline bool is_directory_wide(const std::string& path) {
    std::wstring wpath = utf8_to_wstring(path);
    DWORD attrs = GetFileAttributesW(wpath.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY));
}

// Check if a file exists (handles Unicode)
inline bool file_exists_wide(const std::string& path) {
    std::wstring wpath = utf8_to_wstring(path);
    DWORD attrs = GetFileAttributesW(wpath.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES;
}

#endif // _WIN32
