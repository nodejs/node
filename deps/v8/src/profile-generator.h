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
  virtual ~CodeEntry() { }

  virtual const char* name() = 0;
  INLINE(bool is_js_function());

 protected:
  INLINE(explicit CodeEntry(Logger::LogEventsAndTags tag))
      : tag_(tag) { }

 private:
  Logger::LogEventsAndTags tag_;

  DISALLOW_COPY_AND_ASSIGN(CodeEntry);
};


class StaticNameCodeEntry : public CodeEntry {
 public:
  INLINE(StaticNameCodeEntry(Logger::LogEventsAndTags tag,
                             const char* name));

  INLINE(virtual const char* name()) { return name_ != NULL ? name_ : ""; }

 private:
  const char* name_;

  DISALLOW_COPY_AND_ASSIGN(StaticNameCodeEntry);
};


class ManagedNameCodeEntry : public CodeEntry {
 public:
  INLINE(ManagedNameCodeEntry(Logger::LogEventsAndTags tag,
                              String* name,
                              const char* resource_name, int line_number));

  INLINE(virtual const char* name()) { return !name_.is_empty() ? *name_ : ""; }

 private:
  SmartPointer<char> name_;
  const char* resource_name_;
  int line_number_;

  DISALLOW_COPY_AND_ASSIGN(ManagedNameCodeEntry);
};


class ProfileNode {
 public:
  INLINE(explicit ProfileNode(CodeEntry* entry));

  ProfileNode* FindChild(CodeEntry* entry);
  ProfileNode* FindOrAddChild(CodeEntry* entry);
  INLINE(void IncrementSelfTicks()) { ++self_ticks_; }
  INLINE(void IncreaseTotalTicks(unsigned amount)) { total_ticks_ += amount; }

  INLINE(unsigned total_ticks()) { return total_ticks_; }
  INLINE(unsigned self_ticks()) { return self_ticks_; }

  void Print(int indent);

 private:
  INLINE(static bool CodeEntriesMatch(void* key1, void* key2)) {
    return key1 == key2;
  }

  INLINE(static bool CodeEntryHash(CodeEntry* entry)) {
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


class CpuProfile BASE_EMBEDDED {
 public:
  CpuProfile() { }
  // Add pc -> ... -> main() call path to the profile.
  void AddPath(const Vector<CodeEntry*>& path);
  void CalculateTotalTicks();

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


class ProfileGenerator {
 public:
  ProfileGenerator();
  ~ProfileGenerator();

  CodeEntry* NewCodeEntry(Logger::LogEventsAndTags tag,
                          String* name, String* resource_name, int line_number);
  CodeEntry* NewCodeEntry(Logger::LogEventsAndTags tag, const char* name);

  INLINE(CpuProfile* profile()) { return &profile_; }
  INLINE(CodeMap* code_map()) { return &code_map_; }

 private:
  INLINE(static bool StringsMatch(void* key1, void* key2)) {
    return key1 == key2;
  }

  INLINE(static bool StringEntryHash(String* entry)) {
    return entry->Hash();
  }

  CpuProfile profile_;
  CodeMap code_map_;
  typedef List<CodeEntry*> CodeEntryList;
  CodeEntryList code_entries_;
  // String::Hash -> const char*
  HashMap resource_names_;

  DISALLOW_COPY_AND_ASSIGN(ProfileGenerator);
};


} }  // namespace v8::internal

#endif  // V8_PROFILE_GENERATOR_H_
