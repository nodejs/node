// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev --maglev-disable-builtin-reducers

let dummy;

function round(x) {
  return Math.round(x);
}

function main() {
  const obj = {valueOf: function() { return 16; }};
  for (let i = 0; i < 1e4; ++i) {
    if (round(obj) !== 16) throw "Incorrect result";
  }
}

main();
