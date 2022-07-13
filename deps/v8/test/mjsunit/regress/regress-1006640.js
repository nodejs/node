// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

function main() {
  const v2 = [1337,1337,1337,1337,1337];
  function v9() {
      const v15 = {get:RegExp};
      Object.defineProperty(v2,501,v15);
      const v18 = RegExp();
      const v19 = 1337 instanceof v18;
  }
  const v30 = {defineProperty:Function,get:v9,getPrototypeOf:Object};
  const v32 = new Proxy(ArrayBuffer,v30);
  const v34 = gc(v32);
}

assertThrows(() => main(), TypeError);
