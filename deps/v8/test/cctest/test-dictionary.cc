// Copyright 2011 the V8 project authors. All rights reserved.
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

#include "api.h"
#include "debug.h"
#include "execution.h"
#include "factory.h"
#include "macro-assembler.h"
#include "objects.h"
#include "global-handles.h"
#include "cctest.h"

using namespace v8::internal;


TEST(ObjectHashTable) {
  v8::HandleScope scope;
  LocalContext context;
  Handle<ObjectHashTable> table = FACTORY->NewObjectHashTable(23);
  Handle<JSObject> a = FACTORY->NewJSArray(7);
  Handle<JSObject> b = FACTORY->NewJSArray(11);
  table = PutIntoObjectHashTable(table, a, b);
  CHECK_EQ(table->NumberOfElements(), 1);
  CHECK_EQ(table->Lookup(*a), *b);
  CHECK_EQ(table->Lookup(*b), HEAP->undefined_value());

  // Keys still have to be valid after objects were moved.
  HEAP->CollectGarbage(NEW_SPACE);
  CHECK_EQ(table->NumberOfElements(), 1);
  CHECK_EQ(table->Lookup(*a), *b);
  CHECK_EQ(table->Lookup(*b), HEAP->undefined_value());

  // Keys that are overwritten should not change number of elements.
  table = PutIntoObjectHashTable(table, a, FACTORY->NewJSArray(13));
  CHECK_EQ(table->NumberOfElements(), 1);
  CHECK_NE(table->Lookup(*a), *b);

  // Keys mapped to undefined should be removed permanently.
  table = PutIntoObjectHashTable(table, a, FACTORY->undefined_value());
  CHECK_EQ(table->NumberOfElements(), 0);
  CHECK_EQ(table->NumberOfDeletedElements(), 1);
  CHECK_EQ(table->Lookup(*a), HEAP->undefined_value());

  // Keys should map back to their respective values and also should get
  // an identity hash code generated.
  for (int i = 0; i < 100; i++) {
    Handle<JSObject> key = FACTORY->NewJSArray(7);
    Handle<JSObject> value = FACTORY->NewJSArray(11);
    table = PutIntoObjectHashTable(table, key, value);
    CHECK_EQ(table->NumberOfElements(), i + 1);
    CHECK_NE(table->FindEntry(*key), ObjectHashTable::kNotFound);
    CHECK_EQ(table->Lookup(*key), *value);
    CHECK(key->GetIdentityHash(OMIT_CREATION)->ToObjectChecked()->IsSmi());
  }

  // Keys never added to the map which already have an identity hash
  // code should not be found.
  for (int i = 0; i < 100; i++) {
    Handle<JSObject> key = FACTORY->NewJSArray(7);
    CHECK(key->GetIdentityHash(ALLOW_CREATION)->ToObjectChecked()->IsSmi());
    CHECK_EQ(table->FindEntry(*key), ObjectHashTable::kNotFound);
    CHECK_EQ(table->Lookup(*key), HEAP->undefined_value());
    CHECK(key->GetIdentityHash(OMIT_CREATION)->ToObjectChecked()->IsSmi());
  }

  // Keys that don't have an identity hash should not be found and also
  // should not get an identity hash code generated.
  for (int i = 0; i < 100; i++) {
    Handle<JSObject> key = FACTORY->NewJSArray(7);
    CHECK_EQ(table->Lookup(*key), HEAP->undefined_value());
    CHECK_EQ(key->GetIdentityHash(OMIT_CREATION), HEAP->undefined_value());
  }
}


#ifdef DEBUG
TEST(ObjectHashSetCausesGC) {
  v8::HandleScope scope;
  LocalContext context;
  Handle<ObjectHashSet> table = FACTORY->NewObjectHashSet(1);
  Handle<JSObject> key = FACTORY->NewJSArray(0);

  // Simulate a full heap so that generating an identity hash code
  // in subsequent calls will request GC.
  FLAG_gc_interval = 0;

  // Calling Contains() should not cause GC ever.
  CHECK(!table->Contains(*key));

  // Calling Remove() should not cause GC ever.
  CHECK(!table->Remove(*key)->IsFailure());

  // Calling Add() should request GC by returning a failure.
  CHECK(table->Add(*key)->IsRetryAfterGC());
}
#endif


#ifdef DEBUG
TEST(ObjectHashTableCausesGC) {
  v8::HandleScope scope;
  LocalContext context;
  Handle<ObjectHashTable> table = FACTORY->NewObjectHashTable(1);
  Handle<JSObject> key = FACTORY->NewJSArray(0);

  // Simulate a full heap so that generating an identity hash code
  // in subsequent calls will request GC.
  FLAG_gc_interval = 0;

  // Calling Lookup() should not cause GC ever.
  CHECK(table->Lookup(*key)->IsUndefined());

  // Calling Put() should request GC by returning a failure.
  CHECK(table->Put(*key, *key)->IsRetryAfterGC());
}
#endif
