#include "MemorySegment.hpp"
#include <stdexcept>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

MemorySegment::MemorySegment(const std::string& path)
    : fileMapping(path.c_str(), boost::interprocess::read_only),
      region(fileMapping, boost::interprocess::read_only),
      refcount(1),
      segmentSize(region.get_size()) {}

MemorySegment::~MemorySegment() {}

size_t MemorySegment::size() const {
    return segmentSize;
}

void* MemorySegment::data() {
    return region.get_address();
}

void MemorySegment::incRef() {
    refcount++;
}

void MemorySegment::decRef() {
    refcount--;
}

int MemorySegment::refCount() const {
    return refcount.load();
}
