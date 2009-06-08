// Copyright 2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdlib.h>

#include "v8.h"

namespace v8 {
namespace internal {


void* Malloced::New(size_t size) {
  ASSERT(NativeAllocationChecker::allocation_allowed());
  void* result = malloc(size);
  if (result == NULL) V8::FatalProcessOutOfMemory("Malloced operator new");
  return result;
}


void Malloced::Delete(void* p) {
  free(p);
}


void Malloced::FatalProcessOutOfMemory() {
  V8::FatalProcessOutOfMemory("Out of memory");
}


#ifdef DEBUG

static void* invalid = static_cast<void*>(NULL);

void* Embedded::operator new(size_t size) {
  UNREACHABLE();
  return invalid;
}


void Embedded::operator delete(void* p) {
  UNREACHABLE();
}


void* AllStatic::operator new(size_t size) {
  UNREACHABLE();
  return invalid;
}


void AllStatic::operator delete(void* p) {
  UNREACHABLE();
}

#endif


char* StrDup(const char* str) {
  int length = strlen(str);
  char* result = NewArray<char>(length + 1);
  memcpy(result, str, length * kCharSize);
  result[length] = '\0';
  return result;
}


char* StrNDup(const char* str, size_t n) {
  size_t length = strlen(str);
  if (n < length) length = n;
  char* result = NewArray<char>(length + 1);
  memcpy(result, str, length * kCharSize);
  result[length] = '\0';
  return result;
}


int NativeAllocationChecker::allocation_disallowed_ = 0;


PreallocatedStorage PreallocatedStorage::in_use_list_(0);
PreallocatedStorage PreallocatedStorage::free_list_(0);
bool PreallocatedStorage::preallocated_ = false;


void PreallocatedStorage::Init(size_t size) {
  ASSERT(free_list_.next_ == &free_list_);
  ASSERT(free_list_.previous_ == &free_list_);
  PreallocatedStorage* free_chunk =
      reinterpret_cast<PreallocatedStorage*>(new char[size]);
  free_list_.next_ = free_list_.previous_ = free_chunk;
  free_chunk->next_ = free_chunk->previous_ = &free_list_;
  free_chunk->size_ = size - sizeof(PreallocatedStorage);
  preallocated_ = true;
}


void* PreallocatedStorage::New(size_t size) {
  if (!preallocated_) {
    return FreeStoreAllocationPolicy::New(size);
  }
  ASSERT(free_list_.next_ != &free_list_);
  ASSERT(free_list_.previous_ != &free_list_);
  size = (size + kPointerSize - 1) & ~(kPointerSize - 1);
  // Search for exact fit.
  for (PreallocatedStorage* storage = free_list_.next_;
       storage != &free_list_;
       storage = storage->next_) {
    if (storage->size_ == size) {
      storage->Unlink();
      storage->LinkTo(&in_use_list_);
      return reinterpret_cast<void*>(storage + 1);
    }
  }
  // Search for first fit.
  for (PreallocatedStorage* storage = free_list_.next_;
       storage != &free_list_;
       storage = storage->next_) {
    if (storage->size_ >= size + sizeof(PreallocatedStorage)) {
      storage->Unlink();
      storage->LinkTo(&in_use_list_);
      PreallocatedStorage* left_over =
          reinterpret_cast<PreallocatedStorage*>(
              reinterpret_cast<char*>(storage + 1) + size);
      left_over->size_ = storage->size_ - size - sizeof(PreallocatedStorage);
      ASSERT(size + left_over->size_ + sizeof(PreallocatedStorage) ==
             storage->size_);
      storage->size_ = size;
      left_over->LinkTo(&free_list_);
      return reinterpret_cast<void*>(storage + 1);
    }
  }
  // Allocation failure.
  ASSERT(false);
  return NULL;
}


// We don't attempt to coalesce.
void PreallocatedStorage::Delete(void* p) {
  if (p == NULL) {
    return;
  }
  if (!preallocated_) {
    FreeStoreAllocationPolicy::Delete(p);
    return;
  }
  PreallocatedStorage* storage = reinterpret_cast<PreallocatedStorage*>(p) - 1;
  ASSERT(storage->next_->previous_ == storage);
  ASSERT(storage->previous_->next_ == storage);
  storage->Unlink();
  storage->LinkTo(&free_list_);
}


void PreallocatedStorage::LinkTo(PreallocatedStorage* other) {
  next_ = other->next_;
  other->next_->previous_ = this;
  previous_ = other;
  other->next_ = this;
}


void PreallocatedStorage::Unlink() {
  next_->previous_ = previous_;
  previous_->next_ = next_;
}


PreallocatedStorage::PreallocatedStorage(size_t size)
  : size_(size) {
  previous_ = next_ = this;
}

} }  // namespace v8::internal
