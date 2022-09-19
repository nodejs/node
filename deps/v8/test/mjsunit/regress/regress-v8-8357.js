// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const s = "Umbridge has been reading your mail, Harry."

{
  let monkey_called = false;
  s.__proto__.__proto__[Symbol.replace] =
    () => { monkey_called = true; };
  s.replace(s);
  assertTrue(monkey_called);
}

{
  let monkey_called = false;
  s.__proto__.__proto__[Symbol.search] =
    () => { monkey_called = true; };
  s.search(s);
  assertTrue(monkey_called);
}

{
  let monkey_called = false;
  s.__proto__.__proto__[Symbol.match] =
    () => { monkey_called = true; };
  s.match(s);
  assertTrue(monkey_called);
}
