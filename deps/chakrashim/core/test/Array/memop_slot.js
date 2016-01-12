//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

const size = 100;
function foo() {
  let a = new Array(size);
  let b = new Array(size);
  let c = new Array(size);
  let d = new Array(size);
  let e = new Array(size);
  a.fill(1);
  b.fill(1);
  c.fill(1);
  d.fill(1);
  e.fill(1);

  validSlotMemop = function() {
    let cl = c.length;
    total = 0;
    let _c = c, _d = d;
    // This is valid
    for(let i = 0; i < cl; ++i) {
      _c[i] = _d[i];
    }
  };

  return function slotMemop() {
    let al = a.length;
    total = 0;
    // Right now this is invalid
    for(let i = 0; i < al; ++i) {
      a[i] = b[i];
      e[i] = 0;
    }
    validSlotMemop();
  };
}
const f = foo();
f();
f();
print("PASSED");
