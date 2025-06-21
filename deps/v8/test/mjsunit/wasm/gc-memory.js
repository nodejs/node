// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let kPageSize = 65536;

function allocMems(count, initial, maximum) {
  print(`alloc ${count}`);
  let result = [];
  for (let i = 0; i < count; i++) {
    print(` memory #${i} (initial=${initial}, maximum=${maximum})...`);
    result.push(new WebAssembly.Memory({initial: initial, maximum: maximum}));
  }
  return result;
}

function check(mems, initial) {
  for (m of mems) {
    assertEquals(initial * kPageSize, m.buffer.byteLength);
  }
}

function test(count, initial, maximum) {
  let mems = allocMems(count, initial, maximum);
  check(mems, initial);
}

test(1, 1, 1);
test(1, 1, 2);
test(1, 1, 3);
test(1, 1, 4);

test(2, 1, 1);
test(2, 1, 2);
test(2, 1, 3);
test(2, 1, 4);

test(1, 1, undefined);
test(2, 1, undefined);
test(3, 1, undefined);
test(4, 1, undefined);
