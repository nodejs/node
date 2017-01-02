// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


(function testConstructClassStrict() {
  "use strict";
  class Base {
    constructor(...args) {
      this.baseArgs = args;
    }
    method() { return this.baseArgs; }
  }

  class Child extends Base {
    constructor(...args) {
      super(...args);
      this.childArgs = args;
    }
  }

  class Child2 extends Base {
    constructor(...args) {
      super("extra", ...args);
      this.childArgs = args;
    }
  }

  var c = new Base(...[1, 2, 3]);
  assertInstanceof(c, Base);
  assertEquals([1, 2, 3], c.method());
  assertEquals([1, 2, 3], c.baseArgs);

  c = new Child(...[1, 2, 3]);
  assertInstanceof(c, Child);
  assertInstanceof(c, Base);
  assertEquals([1, 2, 3], c.method());
  assertEquals([1, 2, 3], c.baseArgs);
  assertEquals([1, 2, 3], c.childArgs);

  c = new Child2(...[1, 2, 3]);
  assertInstanceof(c, Child2);
  assertInstanceof(c, Base);
  assertEquals(["extra", 1, 2, 3], c.method());
  assertEquals(["extra", 1, 2, 3], c.baseArgs);
  assertEquals([1, 2, 3], c.childArgs);
})();


(function testConstructSloppy() {
  class Base {
    constructor(...args) {
      this.baseArgs = args;
    }
    method() { return this.baseArgs; }
  }

  class Child extends Base {
    constructor(...args) {
      super(...args);
      this.childArgs = args;
    }
  }

  class Child2 extends Base {
    constructor(...args) {
      super("extra", ...args);
      this.childArgs = args;
    }
  }

  var c = new Base(...[1, 2, 3]);
  assertInstanceof(c, Base);
  assertEquals([1, 2, 3], c.method());
  assertEquals([1, 2, 3], c.baseArgs);

  c = new Child(...[1, 2, 3]);
  assertInstanceof(c, Child);
  assertInstanceof(c, Base);
  assertEquals([1, 2, 3], c.method());
  assertEquals([1, 2, 3], c.baseArgs);
  assertEquals([1, 2, 3], c.childArgs);

  c = new Child2(...[1, 2, 3]);
  assertInstanceof(c, Child2);
  assertInstanceof(c, Base);
  assertEquals(["extra", 1, 2, 3], c.method());
  assertEquals(["extra", 1, 2, 3], c.baseArgs);
  assertEquals([1, 2, 3], c.childArgs);
})();
