// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class MyPromise extends Promise {
  static resolve() {
    return {
      then() {
        throw "then throws";
      }
    };
  }
}

let myIterable = {
  [Symbol.iterator]() {
    return {
      next() {
          return {};
      },
      get return() { return {}; },
    };
  }
};

MyPromise.race(myIterable).then(
  assertUnreachable,
  (e) => { assertEquals(e, "then throws")});
