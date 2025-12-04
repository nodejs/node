// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const template = `class Foo { foo(){} }`

// Keep recursively embedding the template inside itself until we stack
// overflow. This should not segfault.
let s = template;
while (true) {
  try {
    eval(s);
  } catch (e) {
    // A stack overflow exception eventually is expected.
    break;
  }
  s = s.replace("foo(){}", `foo(){ ${s} }`);
}
