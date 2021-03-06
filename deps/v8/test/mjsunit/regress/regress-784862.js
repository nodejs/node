// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test triggers table allocation in large object space. We don't care
// about the result as long as we don't crash.
const array = new Array();
array[0x80000] = 1;
array.unshift({});
assertThrows(() => new WeakMap(array));
