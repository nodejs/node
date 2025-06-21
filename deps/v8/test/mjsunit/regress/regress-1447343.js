// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const o10 = {
    [Symbol.iterator]() {
        let done = 0;
        return {
            next() {
                done++;
                return {
                    "done": done,
                };
            },
        };
    },
};
let long_string;
try {
  long_string = String.prototype.padStart(536870888);
} catch (e) {
  if (!(e instanceof RangeError)) throw e;
  // 32-bit systems have a lower string limit.
  long_string = String.prototype.padStart(268435440);
}

Symbol.constructor = Symbol;
const t21 = Symbol(long_string);
const expected_error = 'Function\\.prototype\\.apply was called on ' +
    'Symbol\\( +\\.\\.\\.<omitted>\\.\\.\\. +\\), ' +
    'which is a symbol and not a function';
assertThrows(
    () => t21(...o10, Symbol.constructor), TypeError,
    new RegExp(expected_error));
