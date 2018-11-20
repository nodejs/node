// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-async-hooks

try {
Object.prototype.__defineGetter__(0, function(){});
assertThrows("x");
} catch(e) { print("Caught: " + e); }

try {
(function() {
  let asyncIds = [], triggerIds = [];
  let ah = async_hooks.createHook({
    init(asyncId, type, triggerAsyncId, resource) {
      if (type !== 'PROMISE') { return; }
      assertThrows("asyncIds.push(asyncId);");
      assertThrows("triggerIds.push(triggerAsyncId)");
    },
  });
  ah.enable();
  async function foo() {}
  foo();
})();
} catch(e) { print("Caught: " + e); }
try {
  var obj = {prop: 7};
  assertThrows("nonexistant(obj)");
} catch(e) { print("Caught: " + e); }
