// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --allow-natives-syntax --interrupt-budget=1024

function check() {
    // Equal prefix.
    assertTrue("asdf".localeCompare("asdf") == 0);
    assertTrue("asd".localeCompare("asdf") < 0);
    assertTrue("asdff".localeCompare("asdf") > 0);

    // Completely Ignorable.
    assertEquals("asdf".localeCompare("asdf\01"), 0);
    assertEquals("asdf".localeCompare("as\01df"), 0);
    assertEquals("asdf".localeCompare("\01asdf"), 0);

    // Chars differ.
    assertTrue("asdf".localeCompare("asdq") < 0);
    assertTrue("asdq".localeCompare("asdf") > 0);

    // Interesting locales.
    assertEquals('ö'.localeCompare('oe', 'de'), -1);
    assertEquals('Ö'.localeCompare('oe', 'de'), -1);
    assertEquals('ö'.localeCompare('o', 'de-u-co-phonebk'), 1);
    assertEquals('ö'.localeCompare('p', 'de-u-co-phonebk'), -1);
    assertEquals('Ö'.localeCompare('o', 'de-u-co-phonebk'), 1);
    assertEquals('Ö'.localeCompare('p', 'de-u-co-phonebk'), -1);
    assertEquals('ö'.localeCompare('o\u0308', 'de-u-co-phonebk'), 0);
    assertEquals('ö'.localeCompare('O\u0308', 'de-u-co-phonebk'), -1);
    assertEquals('Ö'.localeCompare('o\u0308', 'de-u-co-phonebk'), 1);
    assertEquals('Ö'.localeCompare('O\u0308', 'de-u-co-phonebk'), 0);

    assertEquals('ch'.localeCompare('ca', 'cs-CZ'), 1);
    assertEquals('AA'.localeCompare('A-A', 'th'), 0);

    // Invalid locales. Executed on slow path.
    assertTrue("asdf".localeCompare("asdf", 0) == 0);
    assertTrue("asdf".localeCompare("asdf", NaN) == 0);

    // Attempt to hit different cases of the localeCompare fast path.
    assertEquals('aAaaaö'.localeCompare('aaaaaö', 'en-US'), 1);
    assertEquals('aaaaaöA'.localeCompare('aaaaaöa', 'en-US'), 1);
    assertEquals('ab\u0308'.localeCompare('aa\u0308', 'en-US'), 1);
    assertEquals('aA'.localeCompare('aaa', 'en-US'), -1);
    assertEquals('aa'.localeCompare('aAa', 'en-US'), -1);
    assertEquals('aA'.localeCompare('aa', 'en-US'), 1);
    assertEquals('aa'.localeCompare('aA', 'en-US'), -1);
}

// TODO(tebbi): Make isOptimized() from mjsunit.js available in intl tests.
function isOptimized(fun) {
    return (%GetOptimizationStatus(fun) & (1 << 4)) != 0;
}

assertFalse(isOptimized(check));
%PrepareFunctionForOptimization(check);
check();
check();
check();
%OptimizeFunctionOnNextCall(check);
check();
check();
assertTrue(isOptimized(check));
