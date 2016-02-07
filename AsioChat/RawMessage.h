#pragma once
#include <memory>
#include <google\protobuf\stubs\port.h>
using std::shared_ptr;
using google::protobuf::uint8;
struct RawMessage {
    size_t size;
    shared_ptr<uint8> data;
};
