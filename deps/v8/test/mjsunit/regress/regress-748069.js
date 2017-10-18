// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

try {
  var a = 'a'.repeat(1 << 28);
} catch (e) {
  // If the allocation fails, we don't care, because we can't cause the
  // overflow.
}
// Cause an overflow in worst-case calculation for string replacement.
JSON.stringify(a);
