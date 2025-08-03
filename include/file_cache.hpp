//
// Created by fason on 03/08/25.
//

#ifndef FILE_CACHE_HPP
#define FILE_CACHE_HPP

#include <map>
#include <mutex>
#include <string>

#include "http_structs.hpp"

class FileCache {
public:
    struct FileCacheEntry {
        std::string content;
        std::filesystem::file_time_type last_modified;
    };

    static bool get(const std::string& path, FileCacheEntry& entry, const std::filesystem::file_time_type& last_modified);
    static void put(const std::string& path, const std::string& content, const std::filesystem::file_time_type& last_modified);

private:
    static std::map<std::string, FileCacheEntry> s_cache_map;
    static std::mutex s_mutex;
};

#endif //FILE_CACHE_HPP
