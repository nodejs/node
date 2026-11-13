// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Verify that uncaught exceptions thrown during Math.sumPrecise (after
// IteratorClose) preserve their pending exception message (crbug.com/572143833).

let messages = [];
globalThis.onerror = function(msg) {
  messages.push(msg);
  return true;
};

setTimeout(() => {
  Math.sumPrecise("not a number");
}, 0);

setTimeout(() => {
  Math.sumPrecise(["not a number"]);
}, 0);

setTimeout(() => {
  Math.sumPrecise(new Set(["not a number"]));
}, 0);

setTimeout(() => {
  Math.sumPrecise({
    [Symbol.iterator]() {
      return {
        next() {
          return { value: "not a number", done: false };
        },
        return() {
          throw new Error("inner return error");
        }
      };
    }
  });
}, 0);

setTimeout(() => {
  Math.sumPrecise({
    [Symbol.iterator]() {
      return {
        next() {
          return { value: "not a number", done: false };
        },
        get return() {
          throw new Error("inner return getter error");
        }
      };
    }
  });
}, 0);

setTimeout(() => {
  Math.sumPrecise({
    [Symbol.iterator]() {
      return {
        next() {
          return { value: "not a number", done: false };
        },
        return() {
          return 42;
        }
      };
    }
  });
}, 0);

setTimeout(() => {
  globalThis.onerror = null;
  assertEquals(6, messages.length);
  for (const msg of messages) {
    assertTrue(msg.includes("Uncaught TypeError"), msg);
  }
  quit(0);
}, 0);
