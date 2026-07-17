// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class A {
  getValue() {
    return 'A';
  }

  static extend() {
    return class extends this {
      getValue() {
        return 'A.extend:' + super.getValue();
      }
    }
  }
}

class B extends A {
  getValue() {
    return 'B:' + super.getValue();
  }

  static extend() {
    return class extends super.extend() {
      getValue() {
        return 'B.extend:' + super.getValue();
      }
    }
  }

  static extend2() {
    // Have 2 uses of super to test the Scope's cache.
    let x = super.extend();
    return class extends super.extend() {
      getValue() {
        return 'B.extend:' + super.getValue();
      }
    }
  }
}

const C = B.extend();
const c = new C();
assertEquals(c.getValue(), 'B.extend:A.extend:B:A');

const C2 = B.extend2();
const c2 = new C2();
assertEquals(c2.getValue(), 'B.extend:A.extend:B:A');
