// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var val = [0.5];
var arr = [0.5];
for (var i = -1; i < 1; i++) {
  arr[i] = val;
}
assertEquals(val, arr[-1]);
