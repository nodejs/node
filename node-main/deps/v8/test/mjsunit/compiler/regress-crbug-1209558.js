// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --interrupt-budget=100 --predictable


let GC;
(function () {
  let countGC = 0;
  GC = function () {
    if (countGC++ < 50) gc();
  };
})();


var dummy = {};
dummy.index = 1;
delete dummy.index;
GC();
dummy.index = 1;
delete dummy.index;
GC();
dummy.index = 1;
delete dummy.index;
GC();
dummy.index = 1;
delete dummy.index;
GC();
dummy.index = 1;
delete dummy.index;
GC();


function main() {
  const obj = {};
  const anotherobj = { a: 10 };
  obj.d = anotherobj;

  try {
    delete obj.d;
    GC();
  } catch (e) {}

  try { obj.d = obj; } catch (e) {}

  const a = [1];
  try {
    for (var i = 0; i < 10; i++) {
      var s = "";
      s.x = function () { if (typeof i == "object") gaga(a); };
      try { obj[0] = {}; } catch (e) {}
    }
  } catch (e) {}
}

for (var i = 0; i < 50; i++) {
  main();
}
