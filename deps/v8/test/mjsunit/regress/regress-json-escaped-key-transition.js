// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regression test: JSON.parse must not match a hidden-class transition for an
// escaped property key using the raw (undecoded) source bytes. The builder's
// fast transition path compared `chars_ + key.start()` over `key.length()`
// (the *decoded* length), so escaped keys (e.g. "\t", "\uXXXX") collapsed to
// the byte 0x5C and matched a previously-seen "\\" key, making JSON.parse adopt
// the wrong key. Transitions are isolate-global, so a prior parse corrupted a
// later, unrelated parse.

// A single-char escaped key must keep its decoded value even after a sibling
// "\\" (backslash) key has been parsed on the same prefix map.
(function TestEscapedKeyNotRenamedByPriorBackslashKey() {
  function canaryKeys() {
    return Object.keys(JSON.parse('{"\\r":1,"\\f":2}'));
  }
  assertArrayEquals(['\r', '\f'], canaryKeys());
  // Plants a {<prefix>} -> "\" transition in the global transition tree.
  JSON.parse('{"\\r":3,"\\\\":4}');
  // Must be unaffected: "\f" stays U+000C, not U+005C.
  assertArrayEquals(['\r', '\f'], canaryKeys());
})();

// Multi-character escaped key: decoded length 2 must not be compared against
// the first two raw bytes ("\" + "r").
(function TestMultiCharEscapedKey() {
  function keys() {
    return Object.keys(JSON.parse('{"\\t":1,"\\r\\r":2}'));
  }
  assertArrayEquals(['\t', '\r\r'], keys());
  JSON.parse('{"\\t":1,"\\\\r":2}');  // plants the literal "\r" (0x5C 0x72) key
  assertArrayEquals(['\t', '\r\r'], keys());
})();

// \uXXXX-escaped single-character key (here decodes to plain 'A').
(function TestUnicodeEscapedKey() {
  function keys() {
    return Object.keys(JSON.parse('{"x":1,"\\u0041":2}'));
  }
  assertArrayEquals(['x', 'A'], keys());
  JSON.parse('{"x":1,"\\\\":2}');
  assertArrayEquals(['x', 'A'], keys());
})();

// Override/collide: an escaped key must not be renamed onto a real "\" sibling
// and silently destroy a property (key count must be preserved).
(function TestNoPropertyDestruction() {
  // Prime the transition state that previously triggered the collapse.
  JSON.parse('{"p":1,"\\\\":0}');
  JSON.parse('{"p":1,"\\\\":0,"\\t":0}');
  const o = JSON.parse('{"p":1,"\\t":"a","\\\\":"b"}');
  assertEquals(3, Object.keys(o).length);
  assertEquals('a', o['\t']);
  assertEquals('b', o['\\']);
})();

// Self-contained single document: a later sibling object value must not be
// corrupted by an earlier one within the same parse.
(function TestNestedSiblingNotCorrupted() {
  const o = JSON.parse(
      '{"a":{"p":1,"\\\\":9},"b":{"p":1,"\\\\":"A","\\t":7},' +
      '"c":{"p":1,"\\t":7,"\\\\":"B"}}');
  assertEquals(3, Object.keys(o.c).length);
  assertEquals(7, o.c['\t']);
  assertEquals('B', o.c['\\']);
})();

// A plain (unescaped) empty key must still work (regression guard for the
// empty-key_chars path the fix relies on).
(function TestEmptyKeyStillWorks() {
  const o = JSON.parse('{"a":1,"":2,"b":3}');
  assertArrayEquals(['a', '', 'b'], Object.keys(o));
  assertEquals(2, o['']);
})();
