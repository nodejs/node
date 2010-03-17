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

#include "v8.h"

#include "profile-generator-inl.h"


namespace v8 {
namespace internal {


ProfileNode* ProfileNode::FindChild(CodeEntry* entry) {
  HashMap::Entry* map_entry =
      children_.Lookup(entry, CodeEntryHash(entry), false);
  return map_entry != NULL ?
      reinterpret_cast<ProfileNode*>(map_entry->value) : NULL;
}


ProfileNode* ProfileNode::FindOrAddChild(CodeEntry* entry) {
  HashMap::Entry* map_entry =
      children_.Lookup(entry, CodeEntryHash(entry), true);
  if (map_entry->value == NULL) {
    // New node added.
    map_entry->value = new ProfileNode(entry);
  }
  return reinterpret_cast<ProfileNode*>(map_entry->value);
}


void ProfileNode::Print(int indent) {
  OS::Print("%4u %4u %*c %s\n",
            total_ticks_, self_ticks_,
            indent, ' ',
            entry_ != NULL ? entry_->name() : "");
  for (HashMap::Entry* p = children_.Start();
       p != NULL;
       p = children_.Next(p)) {
    reinterpret_cast<ProfileNode*>(p->value)->Print(indent + 2);
  }
}


namespace {

class DeleteNodesCallback {
 public:
  void AfterAllChildrenTraversed(ProfileNode* node) {
    delete node;
  }

  void AfterChildTraversed(ProfileNode*, ProfileNode*) { }
};

}  // namespace


ProfileTree::~ProfileTree() {
  DeleteNodesCallback cb;
  TraverseBreadthFirstPostOrder(&cb);
}


void ProfileTree::AddPathFromEnd(const Vector<CodeEntry*>& path) {
  ProfileNode* node = root_;
  for (CodeEntry** entry = path.start() + path.length() - 1;
       entry != path.start() - 1;
       --entry) {
    if (*entry != NULL) {
      node = node->FindOrAddChild(*entry);
    }
  }
  node->IncrementSelfTicks();
}


void ProfileTree::AddPathFromStart(const Vector<CodeEntry*>& path) {
  ProfileNode* node = root_;
  for (CodeEntry** entry = path.start();
       entry != path.start() + path.length();
       ++entry) {
    if (*entry != NULL) {
      node = node->FindOrAddChild(*entry);
    }
  }
  node->IncrementSelfTicks();
}


namespace {

struct Position {
  Position(ProfileNode* a_node, HashMap::Entry* a_p)
      : node(a_node), p(a_p) { }
  INLINE(ProfileNode* current_child()) {
    return reinterpret_cast<ProfileNode*>(p->value);
  }
  ProfileNode* node;
  HashMap::Entry* p;
};

}  // namespace


template <typename Callback>
void ProfileTree::TraverseBreadthFirstPostOrder(Callback* callback) {
  List<Position> stack(10);
  stack.Add(Position(root_, root_->children_.Start()));
  do {
    Position& current = stack.last();
    if (current.p != NULL) {
      stack.Add(Position(current.current_child(),
                         current.current_child()->children_.Start()));
    } else {
      callback->AfterAllChildrenTraversed(current.node);
      if (stack.length() > 1) {
        Position& parent = stack[stack.length() - 2];
        callback->AfterChildTraversed(parent.node, current.node);
        parent.p = parent.node->children_.Next(parent.p);
        // Remove child from the stack.
        stack.RemoveLast();
      }
    }
  } while (stack.length() > 1 || stack.last().p != NULL);
}


namespace {

class CalculateTotalTicksCallback {
 public:
  void AfterAllChildrenTraversed(ProfileNode* node) {
    node->IncreaseTotalTicks(node->self_ticks());
  }

  void AfterChildTraversed(ProfileNode* parent, ProfileNode* child) {
    parent->IncreaseTotalTicks(child->total_ticks());
  }
};

}  // namespace


// Non-recursive implementation of breadth-first tree traversal.
void ProfileTree::CalculateTotalTicks() {
  CalculateTotalTicksCallback cb;
  TraverseBreadthFirstPostOrder(&cb);
}


void ProfileTree::ShortPrint() {
  OS::Print("root: %u %u\n", root_->total_ticks(), root_->self_ticks());
}


void CpuProfile::AddPath(const Vector<CodeEntry*>& path) {
  top_down_.AddPathFromEnd(path);
  bottom_up_.AddPathFromStart(path);
}


void CpuProfile::CalculateTotalTicks() {
  top_down_.CalculateTotalTicks();
  bottom_up_.CalculateTotalTicks();
}


void CpuProfile::ShortPrint() {
  OS::Print("top down ");
  top_down_.ShortPrint();
  OS::Print("bottom up ");
  bottom_up_.ShortPrint();
}


void CpuProfile::Print() {
  OS::Print("[Top down]:\n");
  top_down_.Print();
  OS::Print("[Bottom up]:\n");
  bottom_up_.Print();
}


const CodeMap::CodeTreeConfig::Key CodeMap::CodeTreeConfig::kNoKey = NULL;
const CodeMap::CodeTreeConfig::Value CodeMap::CodeTreeConfig::kNoValue =
    CodeMap::CodeEntryInfo(NULL, 0);


void CodeMap::AddAlias(Address alias, Address addr) {
  CodeTree::Locator locator;
  if (tree_.Find(addr, &locator)) {
    const CodeEntryInfo& entry_info = locator.value();
    tree_.Insert(alias, &locator);
    locator.set_value(entry_info);
  }
}


CodeEntry* CodeMap::FindEntry(Address addr) {
  CodeTree::Locator locator;
  if (tree_.FindGreatestLessThan(addr, &locator)) {
    // locator.key() <= addr. Need to check that addr is within entry.
    const CodeEntryInfo& entry = locator.value();
    if (addr < (locator.key() + entry.size))
      return entry.entry;
  }
  return NULL;
}


ProfileGenerator::ProfileGenerator()
    : resource_names_(StringsMatch) {
}


static void CodeEntriesDeleter(CodeEntry** entry_ptr) {
  delete *entry_ptr;
}


ProfileGenerator::~ProfileGenerator() {
  for (HashMap::Entry* p = resource_names_.Start();
       p != NULL;
       p = resource_names_.Next(p)) {
    DeleteArray(reinterpret_cast<const char*>(p->value));
  }

  code_entries_.Iterate(CodeEntriesDeleter);
}


CodeEntry* ProfileGenerator::NewCodeEntry(
    Logger::LogEventsAndTags tag,
    String* name,
    String* resource_name, int line_number) {
  const char* cached_resource_name = NULL;
  if (resource_name->IsString()) {
    // As we copy contents of resource names, and usually they are repeated,
    // we cache names by string hashcode.
    HashMap::Entry* cache_entry =
        resource_names_.Lookup(resource_name,
                               StringEntryHash(resource_name),
                               true);
    if (cache_entry->value == NULL) {
      // New entry added.
      cache_entry->value =
          resource_name->ToCString(DISALLOW_NULLS,
                                   ROBUST_STRING_TRAVERSAL).Detach();
    }
    cached_resource_name = reinterpret_cast<const char*>(cache_entry->value);
  }

  CodeEntry* entry = new ManagedNameCodeEntry(tag,
                                              name,
                                              cached_resource_name,
                                              line_number);
  code_entries_.Add(entry);
  return entry;
}


CodeEntry* ProfileGenerator::NewCodeEntry(
    Logger::LogEventsAndTags tag,
    const char* name) {
  CodeEntry* entry = new StaticNameCodeEntry(tag, name);
  code_entries_.Add(entry);
  return entry;
}

} }  // namespace v8::internal
