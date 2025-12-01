#pragma once
#include <cstddef>

// Helper: compute byte range for 'lines' format. Returns false if start_line is beyond EOF.
bool compute_line_byte_range(const char* data, size_t total_size, size_t start_line, size_t max_lines, size_t &start_byte, size_t &bytes_len);
