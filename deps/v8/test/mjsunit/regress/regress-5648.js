// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var iter = {}
iter[Symbol.iterator] = () => ({
  next: () => ({}),
  return: () => {throw 666}
});


function* foo() {
  for (let x of iter) {throw 42}
}
assertThrowsEquals(() => foo().next(), 42);


function* bar() {
  let x;
  { let gaga = () => {x};
    [[x]] = iter;
  }
}
assertThrows(() => bar().next(), TypeError);


function baz() {
  let x;
  { let gaga = () => {x};
    let gugu = () => {gaga};
    [[x]] = iter;
  }
}
assertThrows(baz, TypeError);
