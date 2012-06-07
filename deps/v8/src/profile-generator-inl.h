// Copyright 2010 the V8 project authors. All rights reserved.
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

#ifndef V8_PROFILE_GENERATOR_INL_H_
#define V8_PROFILE_GENERATOR_INL_H_

#include "profile-generator.h"

namespace v8 {
namespace internal {

const char* StringsStorage::GetFunctionName(String* name) {
  return GetFunctionName(GetName(name));
}


const char* StringsStorage::GetFunctionName(const char* name) {
  return strlen(name) > 0 ? name : ProfileGenerator::kAnonymousFunctionName;
}


CodeEntry::CodeEntry(Logger::LogEventsAndTags tag,
                     const char* name_prefix,
                     const char* name,
                     const char* resource_name,
                     int line_number,
                     int security_token_id)
    : tag_(tag),
      name_prefix_(name_prefix),
      name_(name),
      resource_name_(resource_name),
      line_number_(line_number),
      shared_id_(0),
      security_token_id_(security_token_id) {
}


bool CodeEntry::is_js_function_tag(Logger::LogEventsAndTags tag) {
  return tag == Logger::FUNCTION_TAG
      || tag == Logger::LAZY_COMPILE_TAG
      || tag == Logger::SCRIPT_TAG
      || tag == Logger::NATIVE_FUNCTION_TAG
      || tag == Logger::NATIVE_LAZY_COMPILE_TAG
      || tag == Logger::NATIVE_SCRIPT_TAG;
}


ProfileNode::ProfileNode(ProfileTree* tree, CodeEntry* entry)
    : tree_(tree),
      entry_(entry),
      total_ticks_(0),
      self_ticks_(0),
      children_(CodeEntriesMatch) {
}


CodeEntry* ProfileGenerator::EntryForVMState(StateTag tag) {
  switch (tag) {
    case GC:
      return gc_entry_;
    case JS:
    case COMPILER:
    // DOM events handlers are reported as OTHER / EXTERNAL entries.
    // To avoid confusing people, let's put all these entries into
    // one bucket.
    case OTHER:
    case EXTERNAL:
      return program_entry_;
    default: return NULL;
  }
}


HeapEntry* HeapGraphEdge::from() const {
  return const_cast<HeapEntry*>(
      reinterpret_cast<const HeapEntry*>(this - child_index_) - 1);
}


SnapshotObjectId HeapObjectsMap::GetNthGcSubrootId(int delta) {
  return kGcRootsFirstSubrootId + delta * kObjectIdStep;
}


HeapObject* V8HeapExplorer::GetNthGcSubrootObject(int delta) {
  return reinterpret_cast<HeapObject*>(
      reinterpret_cast<char*>(kFirstGcSubrootObject) +
      delta * HeapObjectsMap::kObjectIdStep);
}


int V8HeapExplorer::GetGcSubrootOrder(HeapObject* subroot) {
  return static_cast<int>(
      (reinterpret_cast<char*>(subroot) -
       reinterpret_cast<char*>(kFirstGcSubrootObject)) /
      HeapObjectsMap::kObjectIdStep);
}

} }  // namespace v8::internal

#endif  // V8_PROFILE_GENERATOR_INL_H_
