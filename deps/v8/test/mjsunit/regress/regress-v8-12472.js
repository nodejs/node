// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const manyAs = 'A'.repeat(0x10000);
const manyas = manyAs.toLowerCase();
const re = RegExp('^(?:' + manyas + '|' + manyAs + '|' + manyAs + ')$', 'i');

// Shouldn't crash with a stack overflow.
assertThrows(() => manyas.replace(re, manyAs));
