// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var s = '\u1234-------';
for (var i = 0; i < 17; i++) {
  try {
    s += s;
    s += s;
  } catch (e) {
  }
}
s.replace(/a/g);
