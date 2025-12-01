#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include "../src/LineUtils.hpp"

#define ASSERT_TRUE(cond) if(!(cond)) { std::cerr << "Assertion failed: " << #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; return 1; }

// Reference implementation: splits lines by \n, \r, \r\n, \n\r and returns (start_byte, length) for groups
static std::vector<std::pair<size_t,size_t>> reference_line_ranges(const char* data, size_t total_size) {
    std::vector<std::pair<size_t,size_t>> ranges;
    size_t pos = 0;
    while (pos < total_size) {
        size_t start = pos;
        while (pos < total_size && data[pos] != '\n' && data[pos] != '\r') ++pos;
        size_t end_pos = pos;
        if (pos < total_size && (data[pos] == '\n' || data[pos] == '\r')) {
            char ch = data[pos++];
            if ((ch == '\r' || ch == '\n') && pos < total_size && (data[pos] == '\n' || data[pos] == '\r') && data[pos] != ch) {
                ++pos;
            }
            end_pos = pos;
        }
        ranges.emplace_back(start, end_pos - start);
    }
    if (ranges.empty()) {
        // Special case: empty file -> 0 lines but we want to represent a possible empty line?
        // Keep it empty to align with our compute_line_byte_range semantics (empty -> bytes_len == 0)
    }
    return ranges;
}

static bool reference_compute_line_byte_range(const std::string& s, size_t start_line, size_t max_lines, size_t &start_byte, size_t &bytes_len) {
    auto ranges = reference_line_ranges(s.data(), s.size());
    if (start_line > ranges.size()) return false;
    if (start_line == ranges.size()) {
        start_byte = s.size();
        bytes_len = 0;
        return true;
    }
    start_byte = ranges[start_line].first;
    if (max_lines == 0) { bytes_len = 0; return true; }
    size_t end_idx = std::min(start_line + max_lines, ranges.size());
    size_t end_byte = ranges[end_idx - 1].first + ranges[end_idx - 1].second;
    bytes_len = end_byte - start_byte;
    return true;
}

int main() {
    try {
        // Repeatable RNG
        std::mt19937 rng(123456);
        std::uniform_int_distribution<int> len_d(0, 2048);
        std::uniform_int_distribution<int> char_d(0, 99);

        for (int iter = 0; iter < 2000; ++iter) {
            int len = len_d(rng);
            std::string s;
            s.reserve(len);
            for (int i = 0; i < len; ++i) {
                int r = char_d(rng);
                if (r < 3) {
                    s.push_back('\n');
                } else if (r < 6) {
                    s.push_back('\r');
                } else {
                    // ascii printable
                    char c = (char)(' ' + (r % 95));
                    s.push_back(c);
                }
            }
            auto ranges = reference_line_ranges(s.data(), s.size());
            size_t line_count = ranges.size();
            // sample some start lines
            for (int trial = 0; trial < 20; ++trial) {
                size_t start_line = (line_count == 0) ? 0 : (rng() % (line_count + 1)); // could be equal to line_count -> beyond EOF
                size_t max_lines = (line_count == 0) ? 0 : (rng() % (line_count + 1));
                size_t start_byte1 = 0, bytes_len1 = 0;
                bool ok1 = compute_line_byte_range(s.data(), s.size(), start_line, max_lines, start_byte1, bytes_len1);
                size_t start_byte2 = 0, bytes_len2 = 0;
                bool ok2 = reference_compute_line_byte_range(s, start_line, max_lines, start_byte2, bytes_len2);
                if (ok1 != ok2) {
                    std::cerr << "Mismatch ok: " << ok1 << " vs " << ok2 << " for start_line=" << start_line << " max_lines=" << max_lines << " len=" << len << std::endl;
                    return 1;
                }
                if (ok1) {
                    if (start_byte1 != start_byte2 || bytes_len1 != bytes_len2) {
                        std::cerr << "Mismatch at iter=" << iter << " start_line=" << start_line << " max_lines=" << max_lines << "\n";
                        std::cerr << "expected start_byte=" << start_byte2 << " bytes_len=" << bytes_len2 << " got start_byte=" << start_byte1 << " bytes_len=" << bytes_len1 << "\n";
                        return 1;
                    }
                }
            }
        }

        // A specifically crafted Windows CRLF test (common case)
        std::string crlf = "L1\r\nL2\r\nL3\r\n";
        size_t sb = 0, bl = 0;
        bool ok = compute_line_byte_range(crlf.data(), crlf.size(), 0, 1, sb, bl);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(std::string(crlf.data()+sb, bl) == std::string("L1\r\n"));

        // CRLF with multi-line ranges
        ok = compute_line_byte_range(crlf.data(), crlf.size(), 1, 2, sb, bl);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(std::string(crlf.data()+sb, bl) == std::string("L2\r\nL3\r\n"));

    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    std::cout << "All fuzz tests passed" << std::endl;
    return 0;
}
