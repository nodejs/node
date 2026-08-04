// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Tests for primitive strings.

var str = 'ott';
assertTrue(%StringIteratorProtector());
assertEquals(['o', 't', 't'], [...str]);

// This changes the String prototype. No more tests should be run after this in
// the same instance.
str.__proto__[Symbol.iterator] =
    function() {
      return {next : () => ({value : undefined, done : true})};
    };
assertFalse(%StringIteratorProtector());
assertEquals([], [...str]);
