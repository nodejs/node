// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var x = "1".repeat(32 * 1024 * 1024);
for (var z = x;;) {
  try {
    z += {toString: function() { return x; }};
  } catch (e) {
    break;
  }
}
