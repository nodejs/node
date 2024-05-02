// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setup_proxy() {
  // Mess with the prototype to get funky conversion behavior.
  Function.prototype.__proto__ = new Proxy(setup_proxy, {
    get: async (target, key) => {
      console.log(key);
    }
  });
}

setup_proxy();

function asm(global, imports) {
  'use asm';
  // Trigger proxy trap when looking up #toPrimitive:
  var bar = +imports.bar;
  function f() {}
  return {f: f};
}

assertThrows(() => asm(undefined, {bar: setup_proxy}), TypeError);
