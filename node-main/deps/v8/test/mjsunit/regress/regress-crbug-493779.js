// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-slow-asserts --no-force-slow-path

var s = "\u1234-------";
for (var i = 0; i < 17; i++) {
  s += s;
}
s.replace(/[\u1234]/g, "");
