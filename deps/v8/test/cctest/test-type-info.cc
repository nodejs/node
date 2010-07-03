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


#include "cctest.h"
#include "type-info.h"

namespace v8 {
namespace internal {

TEST(ThreeBitRepresentation) {
  // Numeric types and unknown should fit into the short
  // representation.
  CHECK(TypeInfo::ExpandedRepresentation(
      TypeInfo::Unknown().ThreeBitRepresentation()).IsUnknown());
  CHECK(TypeInfo::ExpandedRepresentation(
      TypeInfo::Number().ThreeBitRepresentation()).IsNumber());
  CHECK(TypeInfo::ExpandedRepresentation(
      TypeInfo::Integer32().ThreeBitRepresentation()).IsInteger32());
  CHECK(TypeInfo::ExpandedRepresentation(
      TypeInfo::Smi().ThreeBitRepresentation()).IsSmi());
  CHECK(TypeInfo::ExpandedRepresentation(
      TypeInfo::Double().ThreeBitRepresentation()).IsDouble());

  // Other types should map to unknown.
  CHECK(TypeInfo::ExpandedRepresentation(
      TypeInfo::Primitive().ThreeBitRepresentation()).IsUnknown());
  CHECK(TypeInfo::ExpandedRepresentation(
      TypeInfo::String().ThreeBitRepresentation()).IsUnknown());
}

} }  // namespace v8::internal
