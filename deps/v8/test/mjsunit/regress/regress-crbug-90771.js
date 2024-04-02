// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Flags: --allow-natives-syntax
function target() {};

for (let key of Object.getOwnPropertyNames(this)) {
  try {
    let newTarget = this[key];
    let arg = target;
    Reflect.construct(target, arg, newTarget);
  } catch {}
}
