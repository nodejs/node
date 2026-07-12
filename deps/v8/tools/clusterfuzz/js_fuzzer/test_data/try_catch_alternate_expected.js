// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* AddTryCatchMutator: Target skip probability 0.9 and toplevel probability 0.9 */
// Original: try_catch.js
function blah() {
  try {
    var a = 10;
    console.log(a);
  } catch (e) {}
  label: for (var i = 0; i < 100; i++) {
    var b = 0;
    while (b < 10) {
      console.log(b);
      b += 2;
      continue label;
    }
  }
  return 1;
}
blah();
blah();
(function () {
  1;
  1;
})();
if (true) {
  2;
  2;
} else {
  3;
  3;
}
let a = 0;
switch (a) {
  case 1:
    1;
}
with (Math) {
  cos(PI);
}
let module = new WebAssembly.Module(builder.toBuffer());
const complex1 = [1, 2, 3];
const complex2 = boom();
let complex3 = function () {
  let complex4 = [1, 2, 3];
  return 2;
}();
if (true) {
  let complex5 = new Map();
}
async function foo(a) {
  let val = await a;
}
try {
  1;
} catch (e) {
  2;
}
call(() => {
  1;
  2;
});
function foo() {
  let a = 0;
  let b = 1;
  boom();
  return {
    a: a,
    b: b
  };
}
let {
  a: x,
  b: y
} = foo();
