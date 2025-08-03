//
// Created by fason on 03/08/25.
//

#include "file_cache.hpp"

#include "logger.hpp"

std::map<std::string, FileCache::FileCacheEntry> FileCache::s_cache_map;
std::mutex FileCache::s_mutex;

bool FileCache::get(const std::string& path, FileCacheEntry& entry, const std::filesystem::file_time_type& last_modified)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (const auto it = s_cache_map.find(path); it != s_cache_map.end() && it->second.last_modified == last_modified) {
        entry = it->second;
        LOG_DEBUG(std::format("Cache hit for {} (size: {})", path, entry.content.size()));
        return true;
    }
    LOG_DEBUG(std::format("Cache miss for {} (not found or stale)", path));
    return false;
}

void FileCache::put(const std::string& path, const std::string& content, const std::filesystem::file_time_type& last_modified)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (content.empty()) {
        LOG_ERROR(std::format("Attempt to cache empty content for {}", path));
        return;
    }
    s_cache_map[path] = {content, last_modified};
    LOG_DEBUG(std::format("Cache updated for {} (size: {})", path, content.size()));
}
