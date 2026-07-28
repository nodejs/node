// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --expose-gc --expose-externalize-string --sandbox-testing

const kInternalizedStringType =
  Sandbox.getInstanceTypeIdFor("INTERNALIZED_ONE_BYTE_STRING_TYPE");
const kConsStringType =
  Sandbox.getInstanceTypeIdFor("CONS_ONE_BYTE_STRING_TYPE");

let first = "first long string to make cons string";
assertEquals(Sandbox.getInstanceTypeIdOf(first), kInternalizedStringType);
let second = "second long string to make cons string";
assertEquals(Sandbox.getInstanceTypeIdOf(second), kInternalizedStringType);
const string = first + second;
assertEquals(Sandbox.getInstanceTypeIdOf(string), kConsStringType);

// String must be in old space for externalization.
assertThrows(() => externalizeString(string));
gc();
gc();

// Traverse to and corrupt the first child's length.
let first_child = Sandbox.getObjectAt(Sandbox.readObjectField(string, 'first'));
assertEquals(first_child, first);
let orig_length = Sandbox.readObjectField(first_child, 'length');
let corrupted_length = Math.floor(Math.random() * 0x100000000);
assertEquals(orig_length, first.length);
Sandbox.corruptObjectField(first_child, 'length', corrupted_length);

// Traverse to and corrupt the second child's length.
let second_child =
    Sandbox.getObjectAt(Sandbox.readObjectField(string, 'second'));
assertEquals(second_child, second);
orig_length = Sandbox.readObjectField(second_child, 'length');
corrupted_length = Math.floor(Math.random() * 0x100000000);
assertEquals(orig_length, second.length);
Sandbox.corruptObjectField(second_child, 'length', corrupted_length);

// Externalization is one way to trigger a WriteToFlat on an external buffer.
externalizeString(string);
