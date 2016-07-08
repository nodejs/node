// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXTERNAL_REFERENCE_TABLE_H_
#define V8_EXTERNAL_REFERENCE_TABLE_H_

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

  int size() const { return refs_.length(); }
  Address address(int i) { return refs_[i].address; }
  const char* name(int i) { return refs_[i].name; }

  inline static Address NotAvailable() { return NULL; }

  static const int kDeoptTableSerializeEntryCount = 64;

 private:
  struct ExternalReferenceEntry {
    Address address;
    const char* name;
  };

  explicit ExternalReferenceTable(Isolate* isolate);

  void Add(Address address, const char* name) {
    ExternalReferenceEntry entry = {address, name};
    refs_.Add(entry);
  }

  List<ExternalReferenceEntry> refs_;

  DISALLOW_COPY_AND_ASSIGN(ExternalReferenceTable);
};

}  // namespace internal
}  // namespace v8
#endif  // V8_EXTERNAL_REFERENCE_TABLE_H_
