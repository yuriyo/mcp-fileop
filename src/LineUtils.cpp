#include "LineUtils.hpp"
#include <cstddef>

bool compute_line_byte_range(const char* data, size_t total_size, size_t start_line, size_t max_lines, size_t &start_byte, size_t &bytes_len) {
	start_byte = 0;
	bytes_len = 0;
	size_t pos = 0;
	size_t current_line = 0;

	// Find the byte index for the start_line
	while (pos < total_size && current_line < start_line) {
		// advance to the end of current line (consume characters until newline)
		while (pos < total_size && data[pos] != '\n' && data[pos] != '\r') ++pos;
		// consume newline sequences (\r, \n, \r\n, \n\r)
		if (pos < total_size && (data[pos] == '\n' || data[pos] == '\r')) {
			char ch = data[pos++];
			if ((ch == '\r' || ch == '\n') && pos < total_size && (data[pos] == '\n' || data[pos] == '\r') && data[pos] != ch) {
				// handle mixed \r\n or \n\r
				++pos;
			}
		}
		++current_line;
	}

	if (current_line < start_line) {
		// start_line beyond EOF
		return false;
	}
	start_byte = pos;

	// If max_lines == 0, return 0 bytes (no lines requested)
	if (max_lines == 0) {
		bytes_len = 0;
		return true;
	}

	// Find end byte after reading max_lines
	size_t lines_read = 0;
	size_t end_pos = pos;
	while (end_pos < total_size && lines_read < max_lines) {
		while (end_pos < total_size && data[end_pos] != '\n' && data[end_pos] != '\r') ++end_pos;
		if (end_pos < total_size && (data[end_pos] == '\n' || data[end_pos] == '\r')) {
			char ch = data[end_pos++];
			if ((ch == '\r' || ch == '\n') && end_pos < total_size && (data[end_pos] == '\n' || data[end_pos] == '\r') && data[end_pos] != ch) {
				++end_pos;
			}
		}
		++lines_read;
	}

	// If end_pos reached EOF but we consumed 0 lines because there were no newline characters
	// and max_lines > 0, we still consider the remaining data as one line (inclusive)
	if (lines_read == 0 && end_pos == total_size && start_byte < total_size) {
		bytes_len = total_size - start_byte;
		return true;
	}

	bytes_len = (end_pos >= start_byte) ? end_pos - start_byte : 0;
	return true;
}
