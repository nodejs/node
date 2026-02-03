// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const numArgs = 100;
const argLength = (1 << 24);
const long_string = "B".repeat(argLength);
const args = [];

for (let i = 0; i < numArgs; i++) {
  args.push(long_string);
}

let threw = false;
try {
  new Worker(() => {}, { type: 'function', arguments: args });
} catch (e) {
  assertTrue(e instanceof Error);
  assertTrue(e.message.includes("Invalid argument"));
  threw = true;
}
assertTrue(threw);
