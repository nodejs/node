// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --expose-gc --sandbox-testing

const kMaxRegularHeapObjectSize = 131072;

// Allocate an array and promote it to the old generation.
const array = Array();
gc();
// Allocate a value in the young generation.
const value = new Number();

// Corrupt the length to still look like a regular heap object.
Sandbox.corruptObjectField(array, 'length', kMaxRegularHeapObjectSize);

// OOB write to the JS array also triggering the write barrier for an old->new
// write with an offset that's too large for the remembered set. This should not
// crash with OOB in malloc'ed memory.
array[array.length - 1] = value;
