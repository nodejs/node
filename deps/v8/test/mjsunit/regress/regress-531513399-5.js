// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation

Object.defineProperty(Number.prototype, 'strictGetterThisType', {
  get() {
    'use strict';
    return typeof this;
  },
  configurable: true
});

class ParentNumber extends Number {}
class ChildNumber extends ParentNumber {
  getSuperStrictThisType() {
    return super.strictGetterThisType;
  }
}

// Strict JS getters (like strict getters defined on Number.prototype) receive
// primitive this unboxed without throwing.
assertEquals('number', ChildNumber.prototype.getSuperStrictThisType.call(42));
