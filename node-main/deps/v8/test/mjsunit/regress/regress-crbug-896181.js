// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var a = new Array();
a[0] = 0.1;
a[2] = 0.2;
Object.defineProperty(a, 1, {
  get: function() {
    a[4] = 0.3;
  },
});

assertSame('0.1,,0.2', a.join());
