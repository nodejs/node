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

#ifdef ENABLE_CPP_PROFILES_PROCESSOR

#include "profile-generator.h"

namespace v8 {
namespace internal {

CodeEntry::CodeEntry(Logger::LogEventsAndTags tag,
                     const char* name_prefix,
                     const char* name,
                     const char* resource_name,
                     int line_number)
    : call_uid_(next_call_uid_++),
      tag_(tag),
      name_prefix_(name_prefix),
      name_(name),
      resource_name_(resource_name),
      line_number_(line_number) {
}


bool CodeEntry::is_js_function_tag(Logger::LogEventsAndTags tag) {
  return tag == Logger::FUNCTION_TAG
      || tag == Logger::LAZY_COMPILE_TAG
      || tag == Logger::SCRIPT_TAG
      || tag == Logger::NATIVE_FUNCTION_TAG
      || tag == Logger::NATIVE_LAZY_COMPILE_TAG
      || tag == Logger::NATIVE_SCRIPT_TAG;
}


ProfileNode::ProfileNode(CodeEntry* entry)
    : entry_(entry),
      total_ticks_(0),
      self_ticks_(0),
      children_(CodeEntriesMatch) {
}


void CodeMap::AddCode(Address addr, CodeEntry* entry, unsigned size) {
  CodeTree::Locator locator;
  tree_.Insert(addr, &locator);
  locator.set_value(CodeEntryInfo(entry, size));
}


void CodeMap::MoveCode(Address from, Address to) {
  tree_.Move(from, to);
}

void CodeMap::DeleteCode(Address addr) {
  tree_.Remove(addr);
}


bool CpuProfilesCollection::is_last_profile() {
  // Called from VM thread, and only it can mutate the list,
  // so no locking is needed here.
  return current_profiles_.length() == 1;
}


const char* CpuProfilesCollection::GetFunctionName(String* name) {
  return GetFunctionName(GetName(name));
}


const char* CpuProfilesCollection::GetFunctionName(const char* name) {
  return strlen(name) > 0 ? name : ProfileGenerator::kAnonymousFunctionName;
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

} }  // namespace v8::internal

#endif  // ENABLE_CPP_PROFILES_PROCESSOR

#endif  // V8_PROFILE_GENERATOR_INL_H_
