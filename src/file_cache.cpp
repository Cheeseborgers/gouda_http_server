//
// Created by fason on 03/08/25.
//

#include "file_cache.hpp"

#include "logger.hpp"

CacheMap FileCache::s_cache_map;
std::list<std::string> FileCache::s_lru_list;
std::mutex FileCache::s_mutex;
size_t FileCache::s_max_size = DEFAULT_MAX_FILE_CACHE_SIZE; // Default max size: 100 entries
size_t FileCache::s_total_size = 0;                         // Track total memory usage

void FileCache::initialize(const size_t max_size)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_max_size = max_size > 0 ? max_size : DEFAULT_MAX_FILE_CACHE_SIZE; // Ensure non-zero max size
    LOG_DEBUG(std::format("FileCache initialized with max size: {} entries", s_max_size));
}

bool FileCache::get(const std::string &path, FileCacheEntry &entry,
                    const std::filesystem::file_time_type &last_modified, const ConnectionId conn_id,
                    const RequestId req_id)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (const auto it = s_cache_map.find(path); it != s_cache_map.end() && it->second.first.last_modified == last_modified) {
        // Move to front of LRU list (most recently used)
        s_lru_list.erase(it->second.second);
        s_lru_list.push_front(path);
        it->second.second = s_lru_list.begin();
        entry = it->second.first;
        LOG_DEBUG(std::format("Client[Connection:{}]: Request[{}]: Cache hit for {} (size: {})", conn_id, req_id, path,
                              entry.content.size()));
        return true;
    }
    LOG_DEBUG(std::format("Client[Connection:{}]: Request[{}]: Cache miss for {} (not found or stale)", conn_id, req_id,
                          path));
    return false;
}

void FileCache::put(const std::string &path, const std::string &content,
                    const std::filesystem::file_time_type &last_modified, const ConnectionId connection_id,
                    const RequestId request_id)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (content.empty()) {
        LOG_ERROR(std::format("Client[Connection:{}]: Request[{}]: Attempt to cache empty content for {}",
                              connection_id, request_id, path));
        return;
    }

    // Remove existing entry if it exists
    if (const auto it = s_cache_map.find(path); it != s_cache_map.end()) {
        s_total_size -= it->second.first.content.size();
        s_lru_list.erase(it->second.second);
        s_cache_map.erase(it);
    }

    // Add new entry
    s_lru_list.push_front(path);
    s_cache_map[path] = {{content, last_modified}, s_lru_list.begin()};
    s_total_size += content.size();
    LOG_DEBUG(std::format("Client[Connection:{}]: Request[{}]: Cache updated for {} (size: {})", connection_id,
                          request_id, path, content.size()));

    // Evict if needed
    evict_if_needed(connection_id, request_id);
}

void FileCache::evict_if_needed(ConnectionId connection_id, RequestId request_id)
{
    while (s_cache_map.size() > s_max_size) {
        // Evict least recently used entry
        const std::string &lru_path = s_lru_list.back();
        if (auto it = s_cache_map.find(lru_path); it != s_cache_map.end()) {
            s_total_size -= it->second.first.content.size();
            LOG_DEBUG(std::format("Client[Connection:{}]: Request[{}]: Evicted cache entry for {} (size: {})",
                                  connection_id, request_id, lru_path, it->second.first.content.size()));
            s_lru_list.pop_back();
            s_cache_map.erase(it);
        }
        else {
            // Should not happen, but clean up list to avoid inconsistency
            s_lru_list.pop_back();
        }
    }
}
