#pragma once

#include <vector>
#include <string>


namespace potree {
  struct Options {
    std::vector<std::string> source;
    std::string encoding = "DEFAULT"; // "BROTLI", "UNCOMPRESSED"
    std::string outdir = "";
    std::string name = "";
    std::string method = "";
    std::string chunkMethod = "";
    std::vector<std::string> attributes;
    bool generatePage = false;
    std::string pageName = "";
    std::string pageTitle = "";
    std::string projection = "";
    bool keepChunks = false;
    bool noChunking = false;
    bool noIndexing = false;
  };
}