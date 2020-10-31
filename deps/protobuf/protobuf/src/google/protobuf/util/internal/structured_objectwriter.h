// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_STRUCTURED_OBJECTWRITER_H__
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_STRUCTURED_OBJECTWRITER_H__

#include <memory>

#include <google/protobuf/stubs/casts.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/util/internal/object_writer.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace util {
namespace converter {

// An StructuredObjectWriter is an ObjectWriter for writing
// tree-structured data in a stream of events representing objects
// and collections. Implementation of this interface can be used to
// write an object stream to an in-memory structure, protobufs,
// JSON, XML, or any other output format desired. The ObjectSource
// interface is typically used as the source of an object stream.
//
// See JsonObjectWriter for a sample implementation of
// StructuredObjectWriter and its use.
//
// Derived classes could be thread-unsafe.
class PROTOBUF_EXPORT StructuredObjectWriter : public ObjectWriter {
 public:
  virtual ~StructuredObjectWriter() {}

 protected:
  // A base element class for subclasses to extend, makes tracking state easier.
  //
  // StructuredObjectWriter behaves as a visitor. BaseElement represents a node
  // in the input tree. Implementation of StructuredObjectWriter should also
  // extend BaseElement to keep track of the location in the input tree.
  class PROTOBUF_EXPORT BaseElement {
   public:
    // Takes ownership of the parent Element.
    explicit BaseElement(BaseElement* parent)
        : parent_(parent), level_(parent == NULL ? 0 : parent->level() + 1) {}
    virtual ~BaseElement() {}

    // Releases ownership of the parent and returns a pointer to it.
    template <typename ElementType>
    ElementType* pop() {
      return down_cast<ElementType*>(parent_.release());
    }

    // Returns true if this element is the root.
    bool is_root() const { return parent_ == nullptr; }

    // Returns the number of hops from this element to the root element.
    int level() const { return level_; }

   protected:
    // Returns pointer to parent element without releasing ownership.
    virtual BaseElement* parent() const { return parent_.get(); }

   private:
    // Pointer to the parent Element.
    std::unique_ptr<BaseElement> parent_;

    // Number of hops to the root Element.
    // The root Element has nullptr parent_ and a level_ of 0.
    const int level_;

    GOOGLE_DISALLOW_IMPLICIT_CONSTRUCTORS(BaseElement);
  };

  StructuredObjectWriter() {}

  // Returns the current element. Used for indentation and name overrides.
  virtual BaseElement* element() = 0;

 private:
  // Do not add any data members to this class.
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(StructuredObjectWriter);
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_STRUCTURED_OBJECTWRITER_H__
