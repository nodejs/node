// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --no-always-turbofan
// Flags: --typed-array-length-loading

var ta = new Int8Array();
for (var i = 0; i < 10000; ++i) {
  i % ta.length;
}
