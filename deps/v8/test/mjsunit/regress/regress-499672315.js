// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Outer {
  #x = 100;
  static create() {
    let storedFunc;
    const C = class Inner extends ({
      getBase() {
        storedFunc = (obj) => obj.#x;
        return Outer;
      }
    }).getBase() {
      #x = 200;
    };
    return { C, storedFunc };
  }
}

const { C, storedFunc } = Outer.create();
const instance = new C();
assertEquals(100, storedFunc(instance));
