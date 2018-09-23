// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

// Flags: --allow-natives-syntax

function useObject(obj) {
  if (!Object.isFrozen(obj)) {
    var t = obj.f;
    obj.f = t * 2;
  }
  return obj.f;
}

var o = {f: 1, g: 2}
assertEquals(useObject(o), 2);
assertEquals(useObject(o), 4);
%OptimizeFunctionOnNextCall(useObject);
Object.freeze(o);
assertEquals(useObject(o), 4);
