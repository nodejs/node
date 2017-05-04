// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXTERNAL_REFERENCE_TABLE_H_
#define V8_EXTERNAL_REFERENCE_TABLE_H_

#include <vector>

#include "src/address-map.h"

namespace v8 {
namespace internal {

class Isolate;

// ExternalReferenceTable is a helper class that defines the relationship
// between external references and their encodings. It is used to build
// hashmaps in ExternalReferenceEncoder and ExternalReferenceDecoder.
class ExternalReferenceTable {
 public:
  static ExternalReferenceTable* instance(Isolate* isolate);
  ~ExternalReferenceTable();

  uint32_t size() const { return static_cast<uint32_t>(refs_.length()); }
  Address address(uint32_t i) { return refs_[i].address; }
  const char* name(uint32_t i) { return refs_[i].name; }
  bool is_api_reference(uint32_t i) { return i >= api_refs_start_; }
  uint32_t num_api_references() { return size() - api_refs_start_; }

#ifdef DEBUG
  void increment_count(uint32_t i) { refs_[i].count++; }
  int count(uint32_t i) { return refs_[i].count; }
  void ResetCount();
  void PrintCount();
#endif  // DEBUG

  static const char* ResolveSymbol(void* address,
                                   std::vector<char**>* = nullptr);

 private:
  struct ExternalReferenceEntry {
    Address address;
    const char* name;
#ifdef DEBUG
    int count;
#endif  // DEBUG
  };

  explicit ExternalReferenceTable(Isolate* isolate);

  void Add(Address address, const char* name) {
#ifdef DEBUG
    ExternalReferenceEntry entry = {address, name, 0};
#else
    ExternalReferenceEntry entry = {address, name};
#endif  // DEBUG
    refs_.Add(entry);
  }

  void AddReferences(Isolate* isolate);
  void AddBuiltins(Isolate* isolate);
  void AddRuntimeFunctions(Isolate* isolate);
  void AddIsolateAddresses(Isolate* isolate);
  void AddAccessors(Isolate* isolate);
  void AddStubCache(Isolate* isolate);
  void AddApiReferences(Isolate* isolate);

  List<ExternalReferenceEntry> refs_;
#ifdef DEBUG
  std::vector<char**> symbol_tables_;
#endif
  uint32_t api_refs_start_;

  DISALLOW_COPY_AND_ASSIGN(ExternalReferenceTable);
};

}  // namespace internal
}  // namespace v8
#endif  // V8_EXTERNAL_REFERENCE_TABLE_H_
