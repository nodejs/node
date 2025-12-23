// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: language/features.js
(() => {
  let v = 2 ** 2;
  v **= 3;
})();
(() => {
  async function* foo1() {
    await 42;
    yield 0;
  }
  async function foo2() {
    for await (let a of b) {
      bar1(x);
    }
  }
})();
(() => {
  /./s;
  /./su;
})();
(() => {
  let v = /(?<month>\d{2})-(?<day>\d{2})/;
  v.exec("11-11").groups.month;
})();
(() => {
  let {
    a,
    ...b
  } = {
    x: 1,
    y: 2,
    z: 3
  };
  let v = {
    a,
    ...b
  };
})();
(() => {
  try {
    throw 0;
  } catch {
    foo();
  } finally {
    bar();
  }
})();
(() => {
  "a b";
})();
(() => {
  let v1 = 6n;
  let v2 = BigInt(1234546) + v1;
  let v3 = 0b1000011n;
})();
(() => {
  object.foo ?? "42";
})();
(() => {
  const obj = {
    foo: {
      bar: 42
    }
  };
  obj?.foo?.bar;
  obj?.meh?.bar;
  obj?.meh?.bar();
})();
(() => {
  let v1 = 1_000_000_000_000;
  let v2 = 0b1010_0001;
})();
(() => {
  let v = false;
  v ||= true;
})();
(() => {
  class Foo {
    #xValue = 0;
    get #x() {
      return this.#xValue;
    }
    set #x(value) {
      this.#xValue = value;
    }
    #inc() {
      this.#x++;
    }
    baz(obj) {
      return #xValue in obj;
    }
  }
})();
(() => {
  class Foo {
    prop = "42";
    fun = () => {
      return this.prop;
    };
    static staticProp = "42";
    static staticFun = function () {
      return Foo2.staticProp;
    };
  }
})();
(() => {
  class Foo {
    static #a = 42;
    static b;
    static {
      this.b = bar(this.#a);
    }
  }
})();
(() => {
  /[\p{Decimal_Number}&&\p{ASCII}]/v;
})();
(() => {
  let v = /(?<month>\d{2})-(?<year>\d{4})|(?<year>\d{4})-(?<month>\d{2})/;
  v.exec("11-1111").groups.year;
})();
(() => {
  /(?i:a)a/;
  /(?m:^a)a/;
  /(?s:.)./;
  /(?im:^a)a/;
})();
(() => {
  let v = do {
    if (v) {
      "1";
    } else {
      "2";
    }
  };
})();
(() => {
  async function foo() {
    using sync = openSync();
    await using async = await openAsync();
  }
})();
