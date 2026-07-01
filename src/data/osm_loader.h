#pragma once

/// @file osm_loader.h
/// @brief Loads preprocessed OSM protobuf data into internal C++ types.

#include <string>
#include <vector>

#include "data/osm_types.h"

/// Reads a preprocessed protobuf binary file and produces osm::OSMData.
class OSMLoader {
public:
    /// Load from a file on disk.
    /// @param path  Path to the .osm_data binary file.
    /// @return Populated OSMData, or an empty OSMData on error (logs to stderr).
    static osm::OSMData load_from_file(const std::string& path);

    /// Load from an in-memory byte buffer.
    static osm::OSMData load_from_bytes(const uint8_t* data, size_t size);

    /// Load from a raw protobuf string.
    static osm::OSMData load_from_string(const std::string& serialised);

private:
    /// Validate and fix bounds after loading.
    static void validate_bounds(osm::OSMData& result);
};
