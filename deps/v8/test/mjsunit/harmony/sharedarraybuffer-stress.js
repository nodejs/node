// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Alloc(size) {
  let b = new SharedArrayBuffer(size);
  let v = new Int32Array(b);
  return {buffer : b, view : v};
}

function RunSomeAllocs(total, retained, size) {
  print(`-------iterations = ${total}, retained = ${retained} -------`);
  var array = new Array(retained);
  for (var i = 0; i < total; i++) {
    let pair = Alloc(size);
    // For some iterations, retain the memory, view, or both.
    switch (i % 3) {
    case 0:
      pair.memory = null;
      break;
    case 1:
      pair.view = null;
      break;
    case 2:
      break;
    }
    array[i % retained] = pair;
  }
}

RunSomeAllocs(10, 1, 1024);
RunSomeAllocs(100, 3, 2048);
RunSomeAllocs(1000, 10, 16384);
RunSomeAllocs(10000, 20, 32768);
