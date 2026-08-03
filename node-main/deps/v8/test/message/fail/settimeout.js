// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var state = 0;
function inc() {
  console.log("increment state");
  state++;
}

function repeat() {
  console.log("current state: " + state);
  if (state < 3) {
    setTimeout(inc, 0);
    setTimeout(repeat, 0);
  } else {
    setTimeout(function() { throw new Error(); });
  }
}

setTimeout(inc, 0);
console.log("state: " + state);
setTimeout(repeat, 0);
console.log("state: " + state);
