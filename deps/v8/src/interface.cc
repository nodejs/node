// Copyright 2012 the V8 project authors. All rights reserved.
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

#include "interface.h"

namespace v8 {
namespace internal {

static bool Match(void* key1, void* key2) {
  String* name1 = *static_cast<String**>(key1);
  String* name2 = *static_cast<String**>(key2);
  ASSERT(name1->IsSymbol());
  ASSERT(name2->IsSymbol());
  return name1 == name2;
}


Interface* Interface::Lookup(Handle<String> name) {
  ASSERT(IsModule());
  ZoneHashMap* map = Chase()->exports_;
  if (map == NULL) return NULL;
  ZoneHashMap::Entry* p = map->Lookup(name.location(), name->Hash(), false);
  if (p == NULL) return NULL;
  ASSERT(*static_cast<String**>(p->key) == *name);
  ASSERT(p->value != NULL);
  return static_cast<Interface*>(p->value);
}


#ifdef DEBUG
// Current nesting depth for debug output.
class Nesting {
 public:
  Nesting()  { current_ += 2; }
  ~Nesting() { current_ -= 2; }
  static int current() { return current_; }
 private:
  static int current_;
};

int Nesting::current_ = 0;
#endif


void Interface::DoAdd(
    void* name, uint32_t hash, Interface* interface, bool* ok) {
  MakeModule(ok);
  if (!*ok) return;

#ifdef DEBUG
  if (FLAG_print_interface_details) {
    PrintF("%*s# Adding...\n", Nesting::current(), "");
    PrintF("%*sthis = ", Nesting::current(), "");
    this->Print(Nesting::current());
    PrintF("%*s%s : ", Nesting::current(), "",
           (*reinterpret_cast<String**>(name))->ToAsciiArray());
    interface->Print(Nesting::current());
  }
#endif

  ZoneHashMap** map = &Chase()->exports_;
  if (*map == NULL) *map = new ZoneHashMap(Match, 8);

  ZoneHashMap::Entry* p = (*map)->Lookup(name, hash, !IsFrozen());
  if (p == NULL) {
    // This didn't have name but was frozen already, that's an error.
    *ok = false;
  } else if (p->value == NULL) {
    p->value = interface;
  } else {
#ifdef DEBUG
    Nesting nested;
#endif
    reinterpret_cast<Interface*>(p->value)->Unify(interface, ok);
  }

#ifdef DEBUG
  if (FLAG_print_interface_details) {
    PrintF("%*sthis' = ", Nesting::current(), "");
    this->Print(Nesting::current());
    PrintF("%*s# Added.\n", Nesting::current(), "");
  }
#endif
}


void Interface::Unify(Interface* that, bool* ok) {
  if (this->forward_) return this->Chase()->Unify(that, ok);
  if (that->forward_) return this->Unify(that->Chase(), ok);
  ASSERT(this->forward_ == NULL);
  ASSERT(that->forward_ == NULL);

  *ok = true;
  if (this == that) return;
  if (this->IsValue()) return that->MakeValue(ok);
  if (that->IsValue()) return this->MakeValue(ok);

#ifdef DEBUG
  if (FLAG_print_interface_details) {
    PrintF("%*s# Unifying...\n", Nesting::current(), "");
    PrintF("%*sthis = ", Nesting::current(), "");
    this->Print(Nesting::current());
    PrintF("%*sthat = ", Nesting::current(), "");
    that->Print(Nesting::current());
  }
#endif

  // Merge the smaller interface into the larger, for performance.
  if (this->exports_ != NULL && (that->exports_ == NULL ||
      this->exports_->occupancy() >= that->exports_->occupancy())) {
    this->DoUnify(that, ok);
  } else {
    that->DoUnify(this, ok);
  }

#ifdef DEBUG
  if (FLAG_print_interface_details) {
    PrintF("%*sthis' = ", Nesting::current(), "");
    this->Print(Nesting::current());
    PrintF("%*sthat' = ", Nesting::current(), "");
    that->Print(Nesting::current());
    PrintF("%*s# Unified.\n", Nesting::current(), "");
  }
#endif
}


void Interface::DoUnify(Interface* that, bool* ok) {
  ASSERT(this->forward_ == NULL);
  ASSERT(that->forward_ == NULL);
  ASSERT(!this->IsValue());
  ASSERT(!that->IsValue());
  ASSERT(*ok);

#ifdef DEBUG
    Nesting nested;
#endif

  // Try to merge all members from that into this.
  ZoneHashMap* map = that->exports_;
  if (map != NULL) {
    for (ZoneHashMap::Entry* p = map->Start(); p != NULL; p = map->Next(p)) {
      this->DoAdd(p->key, p->hash, static_cast<Interface*>(p->value), ok);
      if (!*ok) return;
    }
  }

  // If the new interface is larger than that's, then there were members in
  // 'this' which 'that' didn't have. If 'that' was frozen that is an error.
  int this_size = this->exports_ == NULL ? 0 : this->exports_->occupancy();
  int that_size = map == NULL ? 0 : map->occupancy();
  if (that->IsFrozen() && this_size > that_size) {
    *ok = false;
    return;
  }

  // Merge interfaces.
  this->flags_ |= that->flags_;
  that->forward_ = this;
}


#ifdef DEBUG
void Interface::Print(int n) {
  int n0 = n > 0 ? n : 0;

  if (FLAG_print_interface_details) {
    PrintF("%p", static_cast<void*>(this));
    for (Interface* link = this->forward_; link != NULL; link = link->forward_)
      PrintF("->%p", static_cast<void*>(link));
    PrintF(" ");
  }

  if (IsUnknown()) {
    PrintF("unknown\n");
  } else if (IsValue()) {
    PrintF("value\n");
  } else if (IsModule()) {
    PrintF("module %s{", IsFrozen() ? "" : "(unresolved) ");
    ZoneHashMap* map = Chase()->exports_;
    if (map == NULL || map->occupancy() == 0) {
      PrintF("}\n");
    } else if (n < 0 || n0 >= 2 * FLAG_print_interface_depth) {
      // Avoid infinite recursion on cyclic types.
      PrintF("...}\n");
    } else {
      PrintF("\n");
      for (ZoneHashMap::Entry* p = map->Start(); p != NULL; p = map->Next(p)) {
        String* name = *static_cast<String**>(p->key);
        Interface* interface = static_cast<Interface*>(p->value);
        PrintF("%*s%s : ", n0 + 2, "", name->ToAsciiArray());
        interface->Print(n0 + 2);
      }
      PrintF("%*s}\n", n0, "");
    }
  }
}
#endif

} }  // namespace v8::internal
