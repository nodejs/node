// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --expose-async-hooks --ignore-unhandled-promises

const ah = async_hooks.createHook({});
ah.enable();

import("./does_not_exist.js").then();

function target() {
  isFinite.__proto__.__proto__ = new Proxy(target, {
    get() {
      return Promise.resolve();
  }})
}
target();
