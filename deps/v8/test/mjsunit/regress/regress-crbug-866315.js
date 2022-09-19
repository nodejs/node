// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-async-hooks

let num = 42;
let ah = async_hooks.createHook({});

num.__proto__.__proto__ = ah;
assertThrows('num.enable()');
assertThrows('num.disable()');
