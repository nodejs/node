// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Tests for wrapped strings.

var str = new String('ott');
assertTrue(%StringIteratorProtector());
assertEquals(['o', 't', 't'], [...str]);

function iterator_fn() {
  return {next : () => ({value : undefined, done : true})};
};

str[Symbol.iterator] = iterator_fn;
// This shouldn't invalidate the protector, because it doesn't support String
// objects.
assertTrue(%StringIteratorProtector());
assertEquals([], [...str]);


var str2 = new String('ott');
assertEquals(['o', 't', 't'], [...str2]);
// This changes the String prototype. No more tests should be run after this in
// the same instance.
str2.__proto__[Symbol.iterator] = iterator_fn;
assertFalse(%StringIteratorProtector());
assertEquals([], [...str2]);
