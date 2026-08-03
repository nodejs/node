// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// *********************************** ES2016

(() => {
  // Exponentiation operator
  let v = 2 ** 2;
  v **= 3;
})();

// *********************************** ES2018

(() => {
  // Async generators
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
  // Dotall regexp
  /./s;
  /./su;
})();

(() => {
  // Named capturing groups regex
  let v = /(?<month>\d{2})-(?<day>\d{2})/;
  v.exec("11-11").groups.month;
})();

(() => {
  // Rest
  let { a, ...b } = { x: 1, y: 2, z: 3 };

  // Spread
  let v = { a, ...b };
})();

// *********************************** ES2019

(() => {
  // Optional catch binding
  try {
    throw 0;
  } catch {
    foo();
  } finally {
    bar();
  }
})();

(() => {
  // Json strings - here: "a\u2028b"
  "a b";
})();

// *********************************** ES2020

(() => {
  // BigInts
  let v1 = 6n;
  let v2 = BigInt(1234546) + v1;
  let v3 = 0b1000011n;
})();

(() => {
  // Nullish coalescing operator
  object.foo ?? "42";
})();

(() => {
  // Optional chaining
  const obj = {
    foo: {
      bar: 42,
    },
  };

  obj?.foo?.bar;
  obj?.meh?.bar;
  obj?.meh?.bar();
})();

// *********************************** ES2021

(() => {
// Numeric separator
  let v1 = 1_000_000_000_000;
  let v2 = 0b1010_0001;
})();

(() => {
// Logical assignment operators
  let v = false;
  v ||= true;
})();

// *********************************** ES2022

(() => {
  // Private methods and properties
  // Private property in object
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
  // Class properties
  class Foo {
    prop = "42";
    fun = () => {
      return this.prop;
    };

    static staticProp = "42";
    static staticFun = function() {
      return Foo2.staticProp;
    };
  }
})();

(() => {
  // Class static block
  class Foo {
    static #a = 42;
    static b;
    static {
      this.b = bar(this.#a);
    }
  }
})();

// *********************************** ES2024

(() => {
  // Unicode sets regex
  /[\p{Decimal_Number}&&\p{ASCII}]/v;
})();

// *********************************** ES2025

(() => {
  // Duplicate named capturing groups regex
  let v = /(?<month>\d{2})-(?<year>\d{4})|(?<year>\d{4})-(?<month>\d{2})/;
  v.exec("11-1111").groups.year;
})();

(() => {
  // Regexp modifiers
  /(?i:a)a/;
  /(?m:^a)a/;
  /(?s:.)./;
  /(?im:^a)a/;
})();

// *********************************** Proposals

(() => {
  // Do expressions
  let v = do {
    if (v) {
      ("1");
    } else {
      ("2");
    }
  };
})();

(() => {
  // Explicit resource management
  async function foo() {
    using sync = openSync();
    await using async = await openAsync();
  }
})();
