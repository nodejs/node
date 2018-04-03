// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXTERNAL_REFERENCE_TABLE_H_
#define V8_EXTERNAL_REFERENCE_TABLE_H_

#include <vector>

#include "src/address-map.h"
#include "src/builtins/builtins.h"

namespace v8 {
namespace internal {

class Isolate;

// ExternalReferenceTable is a helper class that defines the relationship
// between external references and their encodings. It is used to build
// hashmaps in ExternalReferenceEncoder and ExternalReferenceDecoder.
class ExternalReferenceTable {
 public:
  static ExternalReferenceTable* instance(Isolate* isolate);

  uint32_t size() const { return static_cast<uint32_t>(refs_.size()); }
  Address address(uint32_t i) { return refs_[i].address; }
  const char* name(uint32_t i) { return refs_[i].name; }

  static const char* ResolveSymbol(void* address);

 private:
  struct ExternalReferenceEntry {
    Address address;
    const char* name;

    ExternalReferenceEntry(Address address, const char* name)
        : address(address), name(name) {}
  };

  explicit ExternalReferenceTable(Isolate* isolate);

  void Add(Address address, const char* name);

  void AddReferences(Isolate* isolate);
  void AddBuiltins(Isolate* isolate);
  void AddRuntimeFunctions(Isolate* isolate);
  void AddIsolateAddresses(Isolate* isolate);
  void AddAccessors(Isolate* isolate);
  void AddStubCache(Isolate* isolate);

  std::vector<ExternalReferenceEntry> refs_;
  DISALLOW_COPY_AND_ASSIGN(ExternalReferenceTable);
};

}  // namespace internal
}  // namespace v8
#endif  // V8_EXTERNAL_REFERENCE_TABLE_H_
