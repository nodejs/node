// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --expose-gc --expose-externalize-string --sandbox-testing

const kSeqStringType = Sandbox.getInstanceTypeIdFor("SEQ_ONE_BYTE_STRING_TYPE");

let string = "foo" + "bar" + "baz";
assertEquals(Sandbox.getInstanceTypeIdOf(string), kSeqStringType);

let orig_length = Sandbox.readObjectField(string, 'length');
assertEquals(orig_length, string.length);
let corrupted_length = Math.floor(Math.random() * 0x100000000);
Sandbox.corruptObjectField(string, 'length', corrupted_length);

// Externalization is one way to trigger a WriteToFlat on an external buffer.
externalizeString(string);
