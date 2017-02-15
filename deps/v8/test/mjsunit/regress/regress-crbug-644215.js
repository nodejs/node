// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var arr = [...[],,];
assertTrue(%HasFastHoleyElements(arr));
assertEquals(1, arr.length);
assertFalse(arr.hasOwnProperty(0));
assertEquals(undefined, arr[0]);
// Should not crash.
assertThrows(() => arr[0][0], TypeError);
