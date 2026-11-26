// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test deserializing varblock DeclarationScopes (BLOCK_SCOPE with
// is_declaration_scope) that carry private_name_lookup_skips_outer_class
// during lazy compilation and constructor expected-property calculation in
// class heritage positions.

// 1. Constructor instantiation (CalculateExpectedNofProperties) inside an arrow
// function with default parameters in a class heritage clause.
{
  class Outer {
    run() {
      class Inner extends ((a = class {}) => {
        function Ctor() {
          Ctor;
        }
        new Ctor();
        () => Inner;
        return Object;
      })() {}
      return new Inner();
    }
  }
  assertTrue(new Outer().run() instanceof Object);
}

// 2. Private name resolution across arrow, function, and class static-block
// varblock scopes in class heritage (no direct eval so the enclosing function
// scope stays contextless and propagates private_name_lookup_skips_outer_class
// to the varblock DeclarationScope).
{
  class Outer {
    #x = 'outer';

    testArrowVarblock() {
      let lazyFn, NestedCls;
      class Inner extends ((a = class {}) => {
        function Ctor(o) {
          Ctor;
          this.val = o.#x;
        }
        lazyFn = (o) => {
          Ctor;
          return o.#x;
        };
        NestedCls = class {
          #y = 'nested';
          static m(o) {
            Ctor;
            return o.#x;
          }
        };
        assertEquals('outer', new Ctor(this).val);
        return Object;
      })() {
        #x = 'inner';
      }

      const inner = new Inner();
      assertEquals('outer', lazyFn(this));
      assertThrows(() => lazyFn(inner), TypeError);
      assertEquals('outer', NestedCls.m(this));
      assertThrows(() => NestedCls.m(inner), TypeError);
    }

    testFunctionVarblock() {
      let lazyFn;
      class Inner extends (function(a = 1) {
        let captured = 42;
        lazyFn = (o) => {
          captured;
          return o.#x;
        };
        return Object;
      })() {
        #x = 'inner';
      }

      assertEquals('outer', lazyFn(this));
      assertThrows(() => lazyFn(new Inner()), TypeError);
    }

    testClassStaticBlockVarblock() {
      let lazyFn;
      class Inner extends (
        class {
          static {
            function readOuterPrivate(o) {
              readOuterPrivate;
              return o.#x;
            }
            lazyFn = readOuterPrivate;
          }
        },
        Object
      ) {
        #x = 'inner';
      }

      assertEquals('outer', lazyFn(this));
      assertThrows(() => lazyFn(new Inner()), TypeError);
    }
  }

  const outer = new Outer();
  outer.testArrowVarblock();
  outer.testFunctionVarblock();
  outer.testClassStaticBlockVarblock();
}
