// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --shared-heap --allow-natives-syntax --fuzzing --shared-strings

const shared_string = %ShareObject("sample string");
assertEquals("sample string", shared_string);
assertTrue(%IsInWritableSharedSpace(shared_string));

const shared_number = %ShareObject(123.45);
assertEquals(123.45, shared_number);
assertTrue(%IsInWritableSharedSpace(shared_number));

const shared_smi = %ShareObject(123);
assertEquals(123, shared_smi);
assertFalse(%IsInWritableSharedSpace(shared_smi));

const shared_bool = %ShareObject(true);
assertTrue(shared_bool);
assertFalse(%IsInWritableSharedSpace(shared_bool));

const shared_null = %ShareObject(null);
assertEquals(null, shared_null);
assertFalse(%IsInWritableSharedSpace(shared_null));

const shared_undefined = %ShareObject(undefined);
assertEquals(undefined, shared_undefined);
assertFalse(%IsInWritableSharedSpace(shared_undefined));

// Negative tests: These objects are non-sharable, when fuzzing, these should
// not cause crashes.
assertEquals(undefined, %ShareObject({}));
assertEquals(undefined, %ShareObject([]));
assertEquals(undefined, %ShareObject(() => null));
assertEquals(undefined, %ShareObject(Math));
// These primitive types aren't supported by the shared heap yet.
assertEquals(undefined, %ShareObject(1n));
assertEquals(undefined, %ShareObject(Symbol("my symbol")));
// Just testing the exposed API.
assertEquals(undefined, %ShareObject()); // too few arguments
assertEquals(1, %ShareObject(1, 2));     // too many arguments
