// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --ignore-unhandled-promises

async function f() {
  var a = [...new Int8Array([, ...new Uint8Array(65536)])];
  var p = new Proxy([f], {
    set: function () { },
    done: undefined.prototype
  });
}

f()
f();
