// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Case A1: Inner class without ClassScope in ScopeInfo, computed property key
// with super.prop inside a method with a context.
{
  class Outer {
    m() {
      let x;
      () => x;
      return class {
        a = 1;
        [super.prop]() {}
        b = ({x: this.a} = null);
      };
    }
  }
  const C = new Outer().m();
  assertThrows(
      () => new C(), TypeError,
      "Cannot destructure property 'x' of 'null' as it is null.");
}

// Case A2: Outer object literal without ObjectLiteralScope in ScopeInfo,
// enclosing method m1 with a context, inner class m2 without context,
// and innermost class with [super.prop] computed property key.
{
  const obj = {
    m1() {
      let x;
      () => x;
      const Outer2 = class {
        m2() {
          return class {
            a = 1;
            [super.prop]() {}
            b = ({x: this.a} = null);
          };
        }
      };
      return new Outer2().m2();
    }
  };
  const C = obj.m1();
  assertThrows(
      () => new C(), TypeError,
      "Cannot destructure property 'x' of 'null' as it is null.");
}

// Case B1: Enclosing static method without context (elided from ScopeInfo),
// inner class with [super.prop] computed method key.
{
  class Outer {
    static m() {
      return class Inner {
        a = 1;
        [super.prop]() {}
        b = ({x: this.a} = null);
      };
    }
  }
  const C = Outer.m();
  assertThrows(
      () => new C(), TypeError,
      "Cannot destructure property 'x' of 'null' as it is null.");
}

// Case B2: Computed property key with super[expr] and computed field with
// super.prop in both instance and static initializers.
{
  class Outer {
    m() {
      return class Inner {
        a = 1;
        [super['prop']] = 2;
        b = ({x: this.a} = null);
      };
    }
    static sm() {
      return class StaticInner {
        static a = 1;
        static [super.prop]() {}
        static {
          ({x: this.a} = null);
        }
      };
    }
  }
  const C = new Outer().m();
  assertThrows(
      () => new C(), TypeError,
      "Cannot destructure property 'x' of 'null' as it is null.");
  assertThrows(
      () => Outer.sm(), TypeError,
      "Cannot destructure property 'x' of 'null' as it is null.");
}

// Case C: super() and new.target in computed property keys when enclosing
// constructor/function scope was elided from ScopeInfo.
{
  class Base {}
  class Derived extends Base {
    constructor() {
      return class Inner {
        a = 1;
        [super()]() {}
        b = ({x: this.a} = null);
      };
    }
  }
  const C1 = new Derived();
  assertThrows(
      () => new C1(), TypeError,
      "Cannot destructure property 'x' of 'null' as it is null.");

  function makeClass() {
    return class Inner {
      a = 1;
      [new.target]() {}
      b = ({x: this.a} = null);
    };
  }
  const C2 = makeClass();
  assertThrows(
      () => new C2(), TypeError,
      "Cannot destructure property 'x' of 'null' as it is null.");
}
