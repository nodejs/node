// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Flags: --enable-experimental-regexp-engine
// Flags: --no-default-to-experimental-regexp-engine
// Flags: --no-force-slow-path

// We shouldn't assign the experimental engine to regexps without 'l' flag.
assertNotEquals("EXPERIMENTAL", %RegexpTypeTag(/asdf/));
assertNotEquals("EXPERIMENTAL", %RegexpTypeTag(/123|asdf/));
assertNotEquals("EXPERIMENTAL", %RegexpTypeTag(/(a*)*x/));
assertNotEquals("EXPERIMENTAL", %RegexpTypeTag(/(a*)\1/));

// We should assign the experimental engine to regexps with 'l' flag.
assertEquals("EXPERIMENTAL", %RegexpTypeTag(/asdf/l));
assertEquals("EXPERIMENTAL", %RegexpTypeTag(/123|asdf/l));
assertEquals("EXPERIMENTAL", %RegexpTypeTag(/(a*)*x/l));

// We should throw if a regexp with 'l' flag can't be handled by the
// experimental engine.
assertThrows(() => /(a*)\1/l, SyntaxError);

// The flags field of a regexp should be sorted.
assertEquals("glmsy", (/asdf/lymsg).flags);

// The 'linear' member should be set according to the linear flag.
assertTrue((/asdf/lymsg).linear);
assertFalse((/asdf/ymsg).linear);

// The new fields installed on the regexp prototype map shouldn't make
// unmodified regexps slow.
assertTrue(%RegexpIsUnmodified(/asdf/));
assertTrue(%RegexpIsUnmodified(/asdf/l));

// Redefined .linear should reflect in flags.
{
  let re = /./;
  Object.defineProperty(re, "linear", { get: function() { return true; } });
  assertEquals("l", re.flags);
}
