#include <iostream>
#include <string>
#include "../src/LineUtils.hpp"

#define ASSERT_TRUE(cond) if(!(cond)) { std::cerr << "Assertion failed: " << #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; return 1; }

int main() {
    try {
        // 1) Empty content
        std::string empty = "";
        size_t start_byte = 0, bytes_len = 0;
        bool ok = compute_line_byte_range(empty.data(), empty.size(), 0, 1, start_byte, bytes_len);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(start_byte == 0);
        ASSERT_TRUE(bytes_len == 0);

        // 2) start_line beyond EOF
        std::string content = "L1\nL2\nL3\n";
        ok = compute_line_byte_range(content.data(), content.size(), 5, 1, start_byte, bytes_len);
        ASSERT_TRUE(!ok);

        // 3) mixed newline sequences, request a single line
        std::string mixed = "A\nB\r\nC\rD\n"; // lines: A, B, C, D
        ok = compute_line_byte_range(mixed.data(), mixed.size(), 1, 1, start_byte, bytes_len); // line 1 -> 'B\r\n'
        ASSERT_TRUE(ok);
        ASSERT_TRUE(mixed.substr(start_byte, bytes_len) == std::string("B\r\n"));

        // 6) CR only newlines
        std::string cr_only = "1\r2\r3\r";
        ok = compute_line_byte_range(cr_only.data(), cr_only.size(), 1, 1, start_byte, bytes_len); // second line
        ASSERT_TRUE(ok);
        ASSERT_TRUE(cr_only.substr(start_byte, bytes_len) == std::string("2\r"));

        // 7) LF only newlines
        std::string lf_only = "1\n2\n3\n";
        ok = compute_line_byte_range(lf_only.data(), lf_only.size(), 2, 1, start_byte, bytes_len);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(lf_only.substr(start_byte, bytes_len) == std::string("3\n"));

        // 8) single long line (no newline); start_line 0, read 1 line should return whole content
        std::string longline(10000, 'x');
        ok = compute_line_byte_range(longline.data(), longline.size(), 0, 1, start_byte, bytes_len);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(bytes_len == longline.size());

        // 4) read range that crosses EOF (max_lines too large) -> read until EOF
        ok = compute_line_byte_range(content.data(), content.size(), 1, 100, start_byte, bytes_len);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(content.substr(start_byte, bytes_len) == std::string("L2\nL3\n"));

        // 5) size == 0 (read 0 lines) should return bytes_len == 0
        ok = compute_line_byte_range(content.data(), content.size(), 1, 0, start_byte, bytes_len);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(bytes_len == 0);

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    std::cout << "All line utils tests passed" << std::endl;
    return 0;
}
