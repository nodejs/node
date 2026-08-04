// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignore-unhandled-promises

const obj = {
  get f() {
    async function f() {
      function g() {
        throw 1;
      }
      g[Symbol.dispose] = g;
      await using y = g;
    }
    return f();
  },
};

async function test(obj) {
  const ser = d8.serializer.serialize(obj);
  const des = d8.serializer.deserialize(ser);
  await des;
}

assertThrowsAsync(test(obj), Error);
