// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Flags: --no-enable-experimental-regexp-engine

// We shouldn't recognize the 'l' flag.
assertThrows(() => new RegExp("asdf", "l"), SyntaxError)
assertThrows(() => new RegExp("123|xyz", "l"), SyntaxError)
assertThrows(() => new RegExp("((a*)*)*", "yls"), SyntaxError)
assertThrows(() => new RegExp("((a*)*)*\1", "l"), SyntaxError)

// RegExps shouldn't have a 'linear' property.
assertFalse(RegExp.prototype.hasOwnProperty('linear'));
assertFalse(/123/.hasOwnProperty('linear'));

{
  let re = /./;
  re.linear = true;
  assertEquals("", re.flags);
}
