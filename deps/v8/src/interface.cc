// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/interface.h"

#include "src/base/lazy-instance.h"

namespace v8 {
namespace internal {

// ---------------------------------------------------------------------------
// Initialization.

struct Interface::Cache {
  template<int flags>
  struct Create {
    static void Construct(Interface* ptr) { ::new (ptr) Interface(flags); }
  };
  typedef Create<VALUE + FROZEN> ValueCreate;
  typedef Create<VALUE + CONST + FROZEN> ConstCreate;

  static base::LazyInstance<Interface, ValueCreate>::type value_interface;
  static base::LazyInstance<Interface, ConstCreate>::type const_interface;
};


base::LazyInstance<Interface, Interface::Cache::ValueCreate>::type
    Interface::Cache::value_interface = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<Interface, Interface::Cache::ConstCreate>::type
    Interface::Cache::const_interface = LAZY_INSTANCE_INITIALIZER;


Interface* Interface::NewValue() {
  return Cache::value_interface.Pointer();  // Cached.
}


Interface* Interface::NewConst() {
  return Cache::const_interface.Pointer();  // Cached.
}


// ---------------------------------------------------------------------------
// Lookup.

Interface* Interface::Lookup(Handle<String> name, Zone* zone) {
  DCHECK(IsModule());
  ZoneHashMap* map = Chase()->exports_;
  if (map == nullptr) return nullptr;
  ZoneAllocationPolicy allocator(zone);
  ZoneHashMap::Entry* p =
      map->Lookup(name.location(), name->Hash(), false, allocator);
  if (p == nullptr) return nullptr;
  DCHECK(*static_cast<String**>(p->key) == *name);
  DCHECK(p->value != nullptr);
  return static_cast<Interface*>(p->value);
}


// ---------------------------------------------------------------------------
// Addition.

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


void Interface::DoAdd(const void* name, uint32_t hash, Interface* interface,
                      Zone* zone, bool* ok) {
  MakeModule(ok);
  if (!*ok) return;

#ifdef DEBUG
  if (FLAG_print_interface_details) {
    PrintF("%*s# Adding...\n", Nesting::current(), "");
    PrintF("%*sthis = ", Nesting::current(), "");
    this->Print(Nesting::current());
    const AstRawString* raw = static_cast<const AstRawString*>(name);
    PrintF("%*s%.*s : ", Nesting::current(), "",
           raw->length(), raw->raw_data());
    interface->Print(Nesting::current());
  }
#endif

  ZoneHashMap** map = &Chase()->exports_;
  ZoneAllocationPolicy allocator(zone);

  if (*map == nullptr) {
    *map = new(zone->New(sizeof(ZoneHashMap)))
        ZoneHashMap(ZoneHashMap::PointersMatch,
                    ZoneHashMap::kDefaultHashMapCapacity, allocator);
  }

  ZoneHashMap::Entry* p =
      (*map)->Lookup(const_cast<void*>(name), hash, !IsFrozen(), allocator);
  if (p == nullptr) {
    // This didn't have name but was frozen already, that's an error.
    *ok = false;
  } else if (p->value == nullptr) {
    p->value = interface;
  } else {
#ifdef DEBUG
    Nesting nested;
#endif
    static_cast<Interface*>(p->value)->Unify(interface, zone, ok);
  }

#ifdef DEBUG
  if (FLAG_print_interface_details) {
    PrintF("%*sthis' = ", Nesting::current(), "");
    this->Print(Nesting::current());
    PrintF("%*s# Added.\n", Nesting::current(), "");
  }
#endif
}


// ---------------------------------------------------------------------------
// Unification.

void Interface::Unify(Interface* that, Zone* zone, bool* ok) {
  if (this->forward_) return this->Chase()->Unify(that, zone, ok);
  if (that->forward_) return this->Unify(that->Chase(), zone, ok);
  DCHECK(this->forward_ == nullptr);
  DCHECK(that->forward_ == nullptr);

  *ok = true;
  if (this == that) return;
  if (this->IsValue()) {
    that->MakeValue(ok);
    if (*ok && this->IsConst()) that->MakeConst(ok);
    return;
  }
  if (that->IsValue()) {
    this->MakeValue(ok);
    if (*ok && that->IsConst()) this->MakeConst(ok);
    return;
  }

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
  if (this->exports_ != nullptr && (that->exports_ == nullptr ||
      this->exports_->occupancy() >= that->exports_->occupancy())) {
    this->DoUnify(that, ok, zone);
  } else {
    that->DoUnify(this, ok, zone);
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


void Interface::DoUnify(Interface* that, bool* ok, Zone* zone) {
  DCHECK(this->forward_ == nullptr);
  DCHECK(that->forward_ == nullptr);
  DCHECK(!this->IsValue());
  DCHECK(!that->IsValue());
  DCHECK(this->index_ == -1);
  DCHECK(that->index_ == -1);
  DCHECK(*ok);

#ifdef DEBUG
    Nesting nested;
#endif

  // Try to merge all members from that into this.
  ZoneHashMap* map = that->exports_;
  if (map != nullptr) {
    for (ZoneHashMap::Entry* p = map->Start(); p != nullptr; p = map->Next(p)) {
      this->DoAdd(p->key, p->hash, static_cast<Interface*>(p->value), zone, ok);
      if (!*ok) return;
    }
  }

  // If the new interface is larger than that's, then there were members in
  // 'this' which 'that' didn't have. If 'that' was frozen that is an error.
  int this_size = this->exports_ == nullptr ? 0 : this->exports_->occupancy();
  int that_size = map == nullptr ? 0 : map->occupancy();
  if (that->IsFrozen() && this_size > that_size) {
    *ok = false;
    return;
  }

  // Merge interfaces.
  this->flags_ |= that->flags_;
  that->forward_ = this;
}


// ---------------------------------------------------------------------------
// Printing.

#ifdef DEBUG
void Interface::Print(int n) {
  int n0 = n > 0 ? n : 0;

  if (FLAG_print_interface_details) {
    PrintF("%p", static_cast<void*>(this));
    for (Interface* link = this->forward_; link != nullptr;
        link = link->forward_) {
      PrintF("->%p", static_cast<void*>(link));
    }
    PrintF(" ");
  }

  if (IsUnknown()) {
    PrintF("unknown\n");
  } else if (IsConst()) {
    PrintF("const\n");
  } else if (IsValue()) {
    PrintF("value\n");
  } else if (IsModule()) {
    PrintF("module %d %s{", Index(), IsFrozen() ? "" : "(unresolved) ");
    ZoneHashMap* map = Chase()->exports_;
    if (map == nullptr || map->occupancy() == 0) {
      PrintF("}\n");
    } else if (n < 0 || n0 >= 2 * FLAG_print_interface_depth) {
      // Avoid infinite recursion on cyclic types.
      PrintF("...}\n");
    } else {
      PrintF("\n");
      for (ZoneHashMap::Entry* p = map->Start();
           p != nullptr; p = map->Next(p)) {
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
