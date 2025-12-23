#pragma once

#include <vector>
#include <string>


namespace potree {
  struct options {
    std::vector<std::string> m_source;
    std::string m_encoding = "DEFAULT"; // "BROTLI", "UNCOMPRESSED"
    std::string m_outdir = "";
    std::string m_name = "";
    std::string m_method = "";
    std::string m_chunk_method = "";
    std::vector<std::string> m_attributes;
    bool m_generate_page = false;
    std::string m_page_name = "";
    std::string m_page_title = "";
    std::string m_projection = "";
    bool m_keep_chunks = false;
    bool m_no_chunking = false;
    bool m_no_indexing = false;

    bool skip_chunking() const { return m_no_chunking || m_chunk_method == "SKIP"; }
  };
}