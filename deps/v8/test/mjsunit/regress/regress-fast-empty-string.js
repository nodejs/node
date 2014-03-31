// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var o = {};
o[""] = 1;
var x = {__proto__:o};
for (i = 0; i < 3; i++) {
  o[""];
}
for (i = 0; i < 3; i++) {
  assertEquals(undefined, o.x);
}
