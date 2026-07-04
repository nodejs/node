// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --expose-gc --expose-externalize-string --sandbox-testing

const kConsStringType =
  Sandbox.getInstanceTypeIdFor("CONS_ONE_BYTE_STRING_TYPE");

let first = "first long string to make cons string";
let second = "second long string to make cons string";
const string = first + second;
assertEquals(Sandbox.getInstanceTypeIdOf(string), kConsStringType);

// String must be in old space for externalization.
assertThrows(() => externalizeString(string));
gc();
gc();

let orig_length = Sandbox.readObjectField(string, 'length');
assertEquals(orig_length, string.length);
let corrupted_length = Math.floor(Math.random() * 0x100000000);
Sandbox.corruptObjectField(string, 'length', corrupted_length);

// Externalization is one way to trigger a WriteToFlat on an external buffer.
externalizeString(string);
