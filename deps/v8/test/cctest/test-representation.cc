// Copyright 2013 the V8 project authors. All rights reserved.
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

#include "test/cctest/cctest.h"

#include "src/objects/property-details.h"

namespace v8 {
namespace internal {

void TestPairPositive(Representation more_general,
              Representation less_general) {
  CHECK(more_general.is_more_general_than(less_general));
}


void TestPairNegative(Representation more_general,
              Representation less_general) {
  CHECK(!more_general.is_more_general_than(less_general));
}


TEST(RepresentationMoreGeneralThan) {
  TestPairNegative(Representation::None(), Representation::None());
  TestPairPositive(Representation::Smi(), Representation::None());
  TestPairPositive(Representation::HeapObject(), Representation::None());
  TestPairPositive(Representation::Double(), Representation::None());
  TestPairPositive(Representation::Tagged(), Representation::None());

  TestPairNegative(Representation::None(), Representation::Smi());
  TestPairNegative(Representation::Smi(), Representation::Smi());
  TestPairNegative(Representation::HeapObject(), Representation::Smi());
  TestPairPositive(Representation::Double(), Representation::Smi());
  TestPairPositive(Representation::Tagged(), Representation::Smi());
}

}  // namespace internal
}  // namespace v8
