// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/property.h"

#include "src/field-type.h"
#include "src/handles-inl.h"
#include "src/objects-inl.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {

std::ostream& operator<<(std::ostream& os,
                         const PropertyAttributes& attributes) {
  os << "[";
  os << (((attributes & READ_ONLY) == 0) ? "W" : "_");    // writable
  os << (((attributes & DONT_ENUM) == 0) ? "E" : "_");    // enumerable
  os << (((attributes & DONT_DELETE) == 0) ? "C" : "_");  // configurable
  os << "]";
  return os;
}

Descriptor Descriptor::DataField(Handle<Name> key, int field_index,
                                 PropertyAttributes attributes,
                                 Representation representation) {
  return DataField(key, field_index, FieldType::Any(key->GetIsolate()),
                   attributes, representation);
}

// Outputs PropertyDetails as a dictionary details.
void PropertyDetails::PrintAsSlowTo(std::ostream& os) {
  os << "(";
  os << (kind() == kData ? "data" : "accessor");
  os << ", dictionary_index: " << dictionary_index();
  os << ", attrs: " << attributes() << ")";
}

// Outputs PropertyDetails as a descriptor array details.
void PropertyDetails::PrintAsFastTo(std::ostream& os, PrintMode mode) {
  os << "(";
  os << (kind() == kData ? "data" : "accessor");
  if (location() == kField) {
    os << " field";
    if (mode & kPrintFieldIndex) {
      os << " " << field_index();
    }
    if (mode & kPrintRepresentation) {
      os << ":" << representation().Mnemonic();
    }
  } else {
    os << " descriptor";
  }
  if (mode & kPrintPointer) {
    os << ", p: " << pointer();
  }
  if (mode & kPrintAttributes) {
    os << ", attrs: " << attributes();
  }
  os << ")";
}

#ifdef OBJECT_PRINT
void PropertyDetails::Print(bool dictionary_mode) {
  OFStream os(stdout);
  if (dictionary_mode) {
    PrintAsSlowTo(os);
  } else {
    PrintAsFastTo(os, PrintMode::kPrintFull);
  }
  os << "\n" << std::flush;
}
#endif

}  // namespace internal
}  // namespace v8
