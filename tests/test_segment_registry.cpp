#include "SegmentRegistry.hpp"
#include <cassert>
#include <iostream>

int main() {
    SegmentRegistry registry;
    // This test expects a file "testfile.bin" to exist in the project root
    auto seg = registry.preload("testfile.bin");
    assert(seg != nullptr);
    std::cout << "Preload success, size: " << seg->size() << std::endl;
    registry.close("testfile.bin");
    std::cout << "Close success" << std::endl;
    return 0;
}
