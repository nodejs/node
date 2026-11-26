// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_V8_H
#define MOCK_V8_H

// Mock V8 types inside v8::internal namespace
namespace v8 {
namespace internal {

template <typename T>
class Tagged {
public:
    T* ptr;
    explicit Tagged(T* val) : ptr(val) {}
    T& operator*() const { return *ptr; }
    T* operator->() const { return ptr; }
};

template <typename T>
class DirectHandle {
public:
    T* ptr;
    explicit DirectHandle(T* val) : ptr(val) {}
    explicit DirectHandle(Tagged<T> val) : ptr(val.ptr) {}
    T& operator*() const { return *ptr; }
    T* operator->() const { return ptr; }
};

template <typename T>
class TaggedMember {
private:
    T* ptr = nullptr;
public:
    TaggedMember() = default;
    Tagged<T> load() const { return Tagged<T>(ptr); }
    void store(Tagged<T> val) { ptr = val.ptr; }
};

// Mock Cast casting helper
template <typename Target, typename Source>
inline Tagged<Target> Cast(Tagged<Source> val) {
    return Tagged<Target>(static_cast<Target*>(val.ptr));
}

} // namespace internal
} // namespace v8

// Mock Object Layouts
class DescriptorArray {
public:
    int length = 0;
};

class Map {
private:
    v8::internal::TaggedMember<DescriptorArray> instance_descriptors_;
public:
    v8::internal::Tagged<DescriptorArray> instance_descriptors() const;
    void set_instance_descriptors(v8::internal::Tagged<DescriptorArray> value);
};

// Helper to allocate array (verifies Return Unification)
v8::internal::DirectHandle<DescriptorArray> AllocateArray(int size);

// Self-assignment updater (verifies Temporal Flow Pruning)
v8::internal::DirectHandle<DescriptorArray> UpdateLoop(v8::internal::DirectHandle<DescriptorArray> desc);

#endif
