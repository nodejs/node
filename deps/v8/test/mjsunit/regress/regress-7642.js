// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-public-fields

const a = [2];
const b = [4];

let log;

class C {
  constructor(...args) {
    log = args;
  }
}

class D extends C {
  field = 42;
  constructor() { super(1) };
}
assertEquals(42, (new D).field);
assertEquals([1], log);

class E extends C {
  field = 42;
  constructor() { super(...a) };
}
assertEquals(42, (new E).field);
assertEquals([2], log);

class F extends C {
  field = 42;
  constructor() { super(1, ...a) };
}
assertEquals(42, (new F).field);
assertEquals([1, 2], log);

class G extends C {
  field = 42;
  constructor() { super(1, ...a, 3) };
}
assertEquals(42, (new G).field);
assertEquals([1, 2, 3], log);

class H extends C {
  field = 42;
  constructor() { super(1, ...a, 3, ...b) };
}
assertEquals(42, (new H).field);
assertEquals([1, 2, 3, 4], log);

class I extends C {
  field = 42;
  constructor() { super(1, ...a, 3, ...b, 5) };
}
assertEquals(42, (new I).field);
assertEquals([1, 2, 3, 4, 5], log);
