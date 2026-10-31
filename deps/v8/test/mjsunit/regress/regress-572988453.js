// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --stress-flush-code

class Outer {
  #a = 'outer_a';
  #b = 'outer_b';

  static make() {
    let fn;
    class Inner extends (() => {
      try {
        throw null;
      } catch (e) {
        fn = (function(o) {
          return o.#a + ':' + o.#b;
        });
      }
      return Object;
    })() {
      #a() {}
      #b() {}
    }
    return {fn, Inner};
  }
}

const {fn, Inner} = Outer.make();
assertEquals('outer_a:outer_b', fn(new Outer()));
for (let i = 0; i < 4; i++) gc();
assertEquals('outer_a:outer_b', fn(new Outer()));
assertThrows(() => fn(new Inner()), TypeError);
