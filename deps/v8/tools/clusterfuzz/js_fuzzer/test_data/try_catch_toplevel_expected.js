// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: try_catch.js
function blah() {
  try {
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
  } catch (e) {}
}
try {
  blah();
} catch (e) {}
try {
  blah();
} catch (e) {}
try {
  (function () {
    1;
    1;
  })();
} catch (e) {}
try {
  if (true) {
    2;
    2;
  } else {
    3;
    3;
  }
} catch (e) {}
let a = 0;
try {
  switch (a) {
    case 1:
      1;
  }
} catch (e) {}
try {
  with (Math) {
    cos(PI);
  }
} catch (e) {}
let module = __wrapTC(() => new WebAssembly.Module(builder.toBuffer()));
const complex1 = __wrapTC(() => [1, 2, 3]);
const complex2 = __wrapTC(() => boom());
let complex3 = __wrapTC(() => function () {
  let complex4 = [1, 2, 3];
  return 2;
}());
try {
  if (true) {
    let complex5 = new Map();
  }
} catch (e) {}
async function foo(a) {
  try {
    let val = await a;
  } catch (e) {}
}
try {
  1;
} catch (e) {
  try {
    2;
  } catch (e) {}
}
try {
  call(() => {
    1;
    2;
  });
} catch (e) {}
function foo() {
  try {
    let a = 0;
    let b = 1;
    boom();
    return {
      a: a,
      b: b
    };
  } catch (e) {
    return {};
  }
}
let {
  a: x,
  b: y
} = __wrapTC(() => foo());
