// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --stress-lazy-source-positions

eval(`
let foo = 5;
let change_and_read_foo = () => {
  foo = 42;
  (function read_foo(){return foo;})();
};
change_and_read_foo();
`);
