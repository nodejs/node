// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var num = 2;
var x = {};
for (let i = 0; i < num; i++) {
  x['a' + i] = 1;
}
var x1 = {};
for (let i = 0; i < num; i++) {
  x1['a' + i] = 1;
}
x1.prop = 1;
x.__defineGetter__("prop", function() {
  let obj = {};
  obj.a0 = 1.5;
  for (let i = 0; i < 1024 + 512; i++) {
    let tmp = {};
    tmp.a0 = 1;
    for (let j = 1; j < num; j++) {
      tmp['a' + j] = 1;
    }
    tmp['p' + i] = 1;
  }
  return 4;
});
x.z = 1;
delete x.z;
var y = {...x};
