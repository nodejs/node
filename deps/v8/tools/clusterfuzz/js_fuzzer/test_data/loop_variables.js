// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test data to determine loop variables and differentiate them from
// non-loop variables. The data explores:
// - three loop statements
// - nested loops
// - nested block statements in loops to test the stack
// - complex loop-test expressions
// - non-variable identifiers in loop tests
// - various variables outside of loop tests

let a = 0;

function __f_0(b, c) {
  for (const d = b; d < 10000; d++) {
    console.log(c);
  }
}

let e = false;

{
  let f = 10;
  while(something) {
    a++;
    if (a > 5) break;
  }

  let g = "";
  do {
    g + "...";
  } while (call() || e > 0);
  console.log(g);
}

let h = 0;
let i = 0;
let j = 0;
let k = 0;

while (call(h)) {
  console.log(i);
  h = i;
  try {
    for (; j < 2; j++) {
      {
        console.log(k);
      }
    }
  } catch(e) {}
  k++;
}

let l = 10000;
let m = "foo";
while(l-- > 0) console.log(m);
