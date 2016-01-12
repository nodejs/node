//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo() {
  const size = 100;
  let a = new Array(size);
  let b = new Array(size);
  let c = new Array(size);
  let d = new Array(size);
  let e = new Array(size);
  a.fill(1);
  b.fill(1);
  c.fill(1);
  d.fill(1);
  // If the extracted bounds check are not removed and we do a memop.
  // This will throw an Uninitialized reg?
  for(let i = 0; i < size; ++i) {
    a[i] = b[i];
    c[i] = d[i];
    e[i] = 0;
  }
}

foo();
foo();
print("PASSED");
