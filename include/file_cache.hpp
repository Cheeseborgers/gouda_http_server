//
// Created by fason on 03/08/25.
//

#ifndef FILE_CACHE_HPP
#define FILE_CACHE_HPP

#include <list>
#include <mutex>
#include <string>

#include "http_structs.hpp"

struct FileCacheEntry {
    std::string content;
    std::filesystem::file_time_type last_modified;
};

using CacheMap = std::unordered_map<std::string, std::pair<FileCacheEntry, std::list<std::string>::iterator>>;

class FileCache {
public:
    static void initialize(size_t max_size);
    static bool get(const std::string &path, FileCacheEntry &entry,
                    const std::filesystem::file_time_type &last_modified, ConnectionId conn_id,
                    RequestId req_id);
    static void put(const std::string &path, const std::string &content,
                    const std::filesystem::file_time_type &last_modified, ConnectionId connection_id,
                    RequestId request_id);

private:
    static void evict_if_needed(ConnectionId connection_id, RequestId request_id);

private:
    static CacheMap s_cache_map;
    static std::list<std::string> s_lru_list;
    static std::mutex s_mutex;

    static size_t s_max_size;   // Maximum number of entries
    static size_t s_total_size; // Current total memory size (bytes)
};

#endif // FILE_CACHE_HPP
