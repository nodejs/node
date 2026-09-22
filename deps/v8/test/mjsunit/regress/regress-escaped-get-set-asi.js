// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep the class in a function to exercise preparsing as well as parsing.
function createClass() {
  return class {
    g\u0065t
    a;
    s\u0065t
    b;
    static g\u0065t
    c;
    static s\u0065t
    d;
  };
}

const C = createClass();
assertEquals(['get', 'a', 'set', 'b', 'c', 'd'], Object.keys(new C()));
assertEquals(['get', 'set'], Object.keys(C));
