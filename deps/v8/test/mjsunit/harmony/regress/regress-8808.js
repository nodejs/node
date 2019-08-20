// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows(() => eval(`
  class Foo {
    #x = 1;
    destructureX() {
      const { #x: x } = this;
      return x;
    }
  }
`), SyntaxError);
