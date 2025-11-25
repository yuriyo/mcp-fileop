#pragma once
#include <string>
#include <atomic>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

class MemorySegment {
public:
    MemorySegment(const std::string& path);
    ~MemorySegment();

    size_t size() const;
    void* data();
    void incRef();
    void decRef();
    int refCount() const;

private:
    boost::interprocess::file_mapping fileMapping;
    boost::interprocess::mapped_region region;
    std::atomic<int> refcount;
    size_t segmentSize;
};
