// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class A {
  static set #a(val) {}
  static test() { this.#a = 1; }
}

A.test.call({});
