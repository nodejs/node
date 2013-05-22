// Copyright 2006-2013 the V8 project authors. All rights reserved.
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

#include <stdlib.h>

#include "v8.h"
#include "cctest.h"
#include "code-stubs.h"


using namespace v8::internal;

#define Types CompareNilICStub::Types

TEST(TypeConstructors) {
  Types types;
  types.Add(CompareNilICStub::MONOMORPHIC_MAP);
  Types types2(types);
  CHECK_EQ(types.ToIntegral(), types2.ToIntegral());
}

TEST(ExternalICStateParsing) {
  Types types;
  types.Add(CompareNilICStub::UNDEFINED);
  CompareNilICStub stub(kNonStrictEquality, kUndefinedValue, types);
  CompareNilICStub stub2(stub.GetExtraICState());
  CHECK_EQ(stub.GetKind(), stub2.GetKind());
  CHECK_EQ(stub.GetNilValue(), stub2.GetNilValue());
  CHECK_EQ(stub.GetTypes().ToIntegral(), stub2.GetTypes().ToIntegral());
}

TEST(SettingTypes) {
  Types state;
  CHECK(state.IsEmpty());
  state.Add(CompareNilICStub::NULL_TYPE);
  CHECK(!state.IsEmpty());
  CHECK(state.Contains(CompareNilICStub::NULL_TYPE));
  CHECK(!state.Contains(CompareNilICStub::UNDEFINED));
  CHECK(!state.Contains(CompareNilICStub::UNDETECTABLE));
  state.Add(CompareNilICStub::UNDEFINED);
  CHECK(state.Contains(CompareNilICStub::UNDEFINED));
  CHECK(state.Contains(CompareNilICStub::NULL_TYPE));
  CHECK(!state.Contains(CompareNilICStub::UNDETECTABLE));
}

TEST(ClearTypes) {
  Types state;
  state.Add(CompareNilICStub::NULL_TYPE);
  state.RemoveAll();
  CHECK(state.IsEmpty());
}

TEST(FullCompare) {
  Types state;
  CHECK(Types::FullCompare() != state);
  state.Add(CompareNilICStub::UNDEFINED);
  CHECK(state != Types::FullCompare());
  state.Add(CompareNilICStub::NULL_TYPE);
  CHECK(state != Types::FullCompare());
  state.Add(CompareNilICStub::UNDETECTABLE);
  CHECK(state == Types::FullCompare());
}
