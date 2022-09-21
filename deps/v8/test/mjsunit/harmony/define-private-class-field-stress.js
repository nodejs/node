// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan

// Single structure, field is inline in the object
{
  class C {
    #x;
    constructor(x) { this.#x = x; }
    x() { return this.#x; }
  }
  %PrepareFunctionForOptimization(C);
  assertSame(new C(1).x(), 1);
  assertSame(new C(2).x(), 2);
  assertSame(new C(3).x(), 3);
  assertSame(new C(4).x(), 4);
  assertSame(new C(5).x(), 5);
  %OptimizeFunctionOnNextCall(C);
  assertSame(new C(10).x(), 10);
}

// Single structure, field is out of line
{
  class C {
    a = (() => this.z = this.q = this.alphabet =
               this[Symbol.toStringTag] = this["ninteen eighty four"] = 42)()
    b = (() => this[47] = this.extra = this.fortran = this.derble =
               this["high quality zebras"] = 73)()
    c = (() => this.moreprops = this.jimbo = this["flabberghast"] = 198)()
    #x;
    constructor(x) { this.#x = x; }
    x() { return this.#x; }
  }

  %PrepareFunctionForOptimization(C);
  assertSame(new C(1).x(), 1);
  assertSame(new C(6000).x(), 6000);
  assertSame(new C(37).x(), 37);
  assertSame(new C(-1).x(), -1);
  assertSame(new C(4900).x(), 4900);
  %OptimizeFunctionOnNextCall(C);
  assertSame(new C(9999).x(), 9999);
}

// Multiple structures, field is inline (probably)
{
  let i = 0;
  class C {
    a = (() => {
      if (i > 1000)
        this.tenEtwo = i;
      if ((i & 0x7F) > 64)
        this.boxing = i;
      if (i > 9000)
        this["It's over nine thousand!"] = i;
    })()
    #x;
    constructor(x) { this.#x = x; }
    x() { return this.#x; }
  }
  %PrepareFunctionForOptimization(C);
  assertSame(new C(1).x(), 1);
  assertSame(new C(256).x(), 256);
  assertSame(new C(36).x(), 36);
  assertSame(new C(16384).x(), 16384);
  assertSame(new C(17).x(), 17);
  %OptimizeFunctionOnNextCall(C);
  assertSame(new C(74).x(), 74);
}

// Multiple structures, field should be out of object
{
  let i = 0;
  class C {
    a = (() => this.z = this.q = this.alphabet =
               this[Symbol.toStringTag] = this["ninteen eighty four"] = 42)()
    b = (() => this[47] = this.extra = this.fortran = this.derble =
               this["high quality zebras"] = 73)()
    c = (() => this.moreprops = this.jimbo = this["flabberghast"] = 198)()
    d = (() => {
      if (i > 1000)
        this.tenEtwo = i;
      if ((i & 0x7F) > 64)
        this.boxing = i;
      if (i > 9000)
        this["It's over nine thousand again!"] = i;
    })()
    #x;
    constructor(x) { this.#x = x; }
    x() { return this.#x; }
  }

  %PrepareFunctionForOptimization(C);
  assertSame(new C(1).x(), 1);
  assertSame(new C(38000).x(), 38000);
  assertSame(new C(9080).x(), 9080);
  assertSame(new C(92).x(), 92);
  assertSame(new C(4000).x(), 4000);
  %OptimizeFunctionOnNextCall(C);
  assertSame(new C(10000).x(), 10000);
}
