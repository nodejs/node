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

#ifndef V8_PROFILE_GENERATOR_H_
#define V8_PROFILE_GENERATOR_H_

#include "hashmap.h"

namespace v8 {
namespace internal {


class CodeEntry {
 public:
  // CodeEntry doesn't own name strings, just references them.
  INLINE(CodeEntry(Logger::LogEventsAndTags tag_,
                   const char* name_,
                   const char* resource_name_,
                   int line_number_));

  INLINE(bool is_js_function());
  INLINE(const char* name()) { return name_; }

 private:
  Logger::LogEventsAndTags tag_;
  const char* name_;
  const char* resource_name_;
  int line_number_;

  DISALLOW_COPY_AND_ASSIGN(CodeEntry);
};


class ProfileNode {
 public:
  INLINE(explicit ProfileNode(CodeEntry* entry));

  ProfileNode* FindChild(CodeEntry* entry);
  ProfileNode* FindOrAddChild(CodeEntry* entry);
  INLINE(void IncrementSelfTicks()) { ++self_ticks_; }
  INLINE(void IncreaseTotalTicks(unsigned amount)) { total_ticks_ += amount; }

  INLINE(CodeEntry* entry() const) { return entry_; }
  INLINE(unsigned total_ticks() const) { return total_ticks_; }
  INLINE(unsigned self_ticks() const) { return self_ticks_; }
  void GetChildren(List<ProfileNode*>* children);

  void Print(int indent);

 private:
  INLINE(static bool CodeEntriesMatch(void* entry1, void* entry2)) {
    return entry1 == entry2;
  }

  INLINE(static uint32_t CodeEntryHash(CodeEntry* entry)) {
    return static_cast<int32_t>(reinterpret_cast<intptr_t>(entry));
  }

  CodeEntry* entry_;
  unsigned total_ticks_;
  unsigned self_ticks_;
  // CodeEntry* -> ProfileNode*
  HashMap children_;

  friend class ProfileTree;

  DISALLOW_COPY_AND_ASSIGN(ProfileNode);
};


class ProfileTree BASE_EMBEDDED {
 public:
  ProfileTree() : root_(new ProfileNode(NULL)) { }
  ~ProfileTree();

  void AddPathFromEnd(const Vector<CodeEntry*>& path);
  void AddPathFromStart(const Vector<CodeEntry*>& path);
  void CalculateTotalTicks();

  ProfileNode* root() { return root_; }

  void ShortPrint();
  void Print() {
    root_->Print(0);
  }

 private:
  template <typename Callback>
  void TraverseBreadthFirstPostOrder(Callback* callback);

  ProfileNode* root_;

  DISALLOW_COPY_AND_ASSIGN(ProfileTree);
};


class CpuProfile {
 public:
  CpuProfile() { }
  // Add pc -> ... -> main() call path to the profile.
  void AddPath(const Vector<CodeEntry*>& path);
  void CalculateTotalTicks();

  INLINE(ProfileTree* top_down()) { return &top_down_; }
  INLINE(ProfileTree* bottom_up()) { return &bottom_up_; }

  void ShortPrint();
  void Print();

 private:
  ProfileTree top_down_;
  ProfileTree bottom_up_;

  DISALLOW_COPY_AND_ASSIGN(CpuProfile);
};


class CodeMap BASE_EMBEDDED {
 public:
  CodeMap() { }
  INLINE(void AddCode(Address addr, CodeEntry* entry, unsigned size));
  INLINE(void MoveCode(Address from, Address to));
  INLINE(void DeleteCode(Address addr));
  void AddAlias(Address alias, Address addr);
  CodeEntry* FindEntry(Address addr);

 private:
  struct CodeEntryInfo {
    CodeEntryInfo(CodeEntry* an_entry, unsigned a_size)
        : entry(an_entry), size(a_size) { }
    CodeEntry* entry;
    unsigned size;
  };

  struct CodeTreeConfig {
    typedef Address Key;
    typedef CodeEntryInfo Value;
    static const Key kNoKey;
    static const Value kNoValue;
    static int Compare(const Key& a, const Key& b) {
      return a < b ? -1 : (a > b ? 1 : 0);
    }
  };
  typedef SplayTree<CodeTreeConfig> CodeTree;

  CodeTree tree_;

  DISALLOW_COPY_AND_ASSIGN(CodeMap);
};


class CpuProfilesCollection {
 public:
  CpuProfilesCollection();
  ~CpuProfilesCollection();

  void AddProfile(unsigned uid);

  CodeEntry* NewCodeEntry(Logger::LogEventsAndTags tag,
                          String* name, String* resource_name, int line_number);
  CodeEntry* NewCodeEntry(Logger::LogEventsAndTags tag, const char* name);
  CodeEntry* NewCodeEntry(Logger::LogEventsAndTags tag, int args_count);

  INLINE(CpuProfile* profile()) { return profiles_.last(); }

 private:
  const char* GetName(String* name);
  const char* GetName(int args_count);

  INLINE(static bool StringsMatch(void* key1, void* key2)) {
    return strcmp(reinterpret_cast<char*>(key1),
                  reinterpret_cast<char*>(key2)) == 0;
  }

  // String::Hash -> const char*
  HashMap function_and_resource_names_;
  // args_count -> char*
  List<char*> args_count_names_;
  List<CodeEntry*> code_entries_;
  List<CpuProfile*> profiles_;

  DISALLOW_COPY_AND_ASSIGN(CpuProfilesCollection);
};


class ProfileGenerator {
 public:
  explicit ProfileGenerator(CpuProfilesCollection* profiles);

  INLINE(CodeEntry* NewCodeEntry(Logger::LogEventsAndTags tag,
                                 String* name,
                                 String* resource_name,
                                 int line_number)) {
    return profiles_->NewCodeEntry(tag, name, resource_name, line_number);
  }

  INLINE(CodeEntry* NewCodeEntry(Logger::LogEventsAndTags tag,
                                 const char* name)) {
    return profiles_->NewCodeEntry(tag, name);
  }

  INLINE(CodeEntry* NewCodeEntry(Logger::LogEventsAndTags tag,
                                 int args_count)) {
    return profiles_->NewCodeEntry(tag, args_count);
  }

  void RecordTickSample(const TickSample& sample);

  INLINE(CodeMap* code_map()) { return &code_map_; }

 private:
  INLINE(CpuProfile* profile()) { return profiles_->profile(); }

  CpuProfilesCollection* profiles_;
  CodeMap code_map_;

  DISALLOW_COPY_AND_ASSIGN(ProfileGenerator);
};


} }  // namespace v8::internal

#endif  // V8_PROFILE_GENERATOR_H_
