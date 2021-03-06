// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function AllocMemory(pages, max = pages) {
  let m =
      new WebAssembly.Memory({initial : pages, maximum : max, shared : true});
  let v = new Int32Array(m.buffer);
  return {memory : m, view : v};
}

function RunSomeAllocs(total, retained, pages, max = pages) {
  print(`-------iterations = ${total}, retained = ${retained} -------`);
  var array = new Array(retained);
  for (var i = 0; i < total; i++) {
    if ((i % 25) == 0)
      print(`iteration ${i}`);
    let pair = AllocMemory(pages, max);
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

RunSomeAllocs(10, 1, 1, 1);
RunSomeAllocs(100, 3, 1, 1);
RunSomeAllocs(1000, 10, 1, 1);
RunSomeAllocs(10000, 20, 1, 1);
