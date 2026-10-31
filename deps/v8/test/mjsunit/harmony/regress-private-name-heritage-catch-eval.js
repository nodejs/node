// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test private name resolution in class heritage clauses across catch scopes
// and direct eval calls.

class Outer {
  #x = 'outer';

  testCatchInHeritage() {
    let f1, f2, evalRes;
    class Inner extends (() => {
      try {
        throw null;
      } catch (e) {
        f1 = () => this.#x;
        f2 = () => () => this.#x;
        evalRes = eval('this.#x');
      }
      return Object;
    })() {
      #x = 'inner';
    }
    assertEquals('outer', f1());
    assertEquals('outer', f2()());
    assertEquals('outer', evalRes);
  }

  testDirectEvalInHeritage() {
    let res1, res2, bodyFn;
    class D1 extends (res1 = eval('this.#x'), Object) {
      [(
        assertThrows(() => eval('this.#x'), TypeError),
        bodyFn = (o) => eval('o.#x'),
        'prop'
      )] = 1;
      #x = 'D1';
    }
    assertEquals('outer', res1);
    assertEquals('D1', bodyFn(new D1()));
    assertThrows(() => bodyFn(this), TypeError);

    class D2 extends (() => {
      res2 = eval('this.#x');
      return Object;
    })() {
      #x = 'D2';
    }
    assertEquals('outer', res2);
  }

  testNestedEvalAndLazyFunctionsInHeritage() {
    let nested1, nested2, lazyFn1, lazyFn2, lazyArrow1, lazyArrow2;
    class D1 extends (
      nested1 = eval("eval('this.#x')"),
      lazyFn1 = eval('(function() { return this.#x; })'),
      lazyArrow1 = eval('(() => () => this.#x)()'),
      Object
    ) {
      #x = 'D1';
    }
    assertEquals('outer', nested1);
    assertEquals('outer', lazyFn1.call(this));
    assertEquals('outer', lazyArrow1());

    class D2 extends (() => {
      nested2 = eval("eval('this.#x')");
      lazyFn2 = eval('(function() { return this.#x; })');
      lazyArrow2 = eval('(() => () => this.#x)()');
      return Object;
    })() {
      #x = 'D2';
    }
    assertEquals('outer', nested2);
    assertEquals('outer', lazyFn2.call(this));
    assertEquals('outer', lazyArrow2());
  }

  testContextlessClassInHeritage() {
    let res1, res2;
    class D extends (
      class {
        static [res1 = eval('this.#x')] = 1;
      },
      class extends (res2 = eval('this.#x'), Object) {},
      Object
    ) {
      #x = 'D';
    }
    assertEquals('outer', res1);
    assertEquals('outer', res2);

    let bodyFn;
    class E {
      #x = 'E';
      static [class extends (bodyFn = (o) => eval('o.#x'), Object) {}] = 1;
    }
    assertEquals('E', bodyFn(new E()));
    assertThrows(() => bodyFn(this), TypeError);
  }

  testArrowParamReparentInHeritage() {
    class Middle {
      #x = 'middle';
      run() {
        let res;
        class Inner extends ((
          a = class {
            #y = 1;
            static [res = this.#x] = 2;
          }
        ) => {
          () => a;
          return Object;
        })() {
          #x = 'inner';
        }
        return res;
      }
    }
    assertEquals('middle', new Middle().run());
  }
}

const outer = new Outer();
outer.testCatchInHeritage();
outer.testDirectEvalInHeritage();
outer.testNestedEvalAndLazyFunctionsInHeritage();
outer.testContextlessClassInHeritage();
outer.testArrowParamReparentInHeritage();
