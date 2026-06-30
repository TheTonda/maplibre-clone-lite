// test_mvt.cpp — Simple test program for the MVT parser
// Usage: ./test_mvt <path/to/tile.mvt>
#include "mvt_parser.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <file.mvt>\n", argv[0]);
        return 1;
    }

    // Read entire file into memory
    std::ifstream file(argv[1], std::ios::binary | std::ios::ate);
    if (!file) {
        std::fprintf(stderr, "Error: cannot open file '%s'\n", argv[1]);
        return 1;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(static_cast<size_t>(size));
    if (!file.read(buffer.data(), size)) {
        std::fprintf(stderr, "Error: failed to read file '%s'\n", argv[1]);
        return 1;
    }

    std::printf("Read %zd bytes from '%s'\n\n", size, argv[1]);

    // Parse using protobuf-lite ArrayInputStream + CodedInputStream
    google::protobuf::io::ArrayInputStream array_input(buffer.data(),
                                                        static_cast<int>(size));
    google::protobuf::io::CodedInputStream coded_input(&array_input);

    mvt::Tile tile = mvt::parse_tile(&coded_input);
    mvt::print_summary(tile);

    return 0;
}
