
// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-async-hooks --ignore-unhandled-promises --stack-size=100

async_hooks.createHook({ promiseResolve() { throw new Error(); } }).enable()

import("./does_not_exist.js").then();
function target() {
  isFinite.__proto__.__proto__ = new Proxy(target, {
    get() { return Promise.resolve(); }
  })
}
target();

function runNearStackLimit(f) {
  function t() {
    try {
      return t();
    } catch (e) {
      return f();
    }
  }
  return t();
}

function __f_2() {
  return runNearStackLimit(() => {
      return new Promise(function () {
    });
  });
}
__f_2().then();
__f_2().then();
