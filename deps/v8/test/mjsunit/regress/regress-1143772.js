// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

(function() {
    // Only run this test if doubles are transitioned in-place to tagged.
    let x = {};
    x.a = 0.1;
    let y = {};
    y.a = {};
    if (!%HaveSameMap(x, y)) return;

    // m1: {}
    let m1 = {};

    // m2: {a:d}
    let m2 = {};
    assertTrue(%HaveSameMap(m2, m1));
    m2.a = 13.37;

    // m3: {a:d, b:s}
    let m3 = {};
    m3.a = 13.37;
    assertTrue(%HaveSameMap(m3, m2));
    m3.b = 1;

    // m4: {a:d, b:s, c:h}
    let m4 = {};
    m4.a = 13.37;
    m4.b = 1;
    assertTrue(%HaveSameMap(m4, m3));
    m4.c = {};

    // m4_2 == m4
    let m4_2 = {};
    m4_2.a = 13.37;
    m4_2.b = 1;
    m4_2.c = {};
    assertTrue(%HaveSameMap(m4_2, m4));

    // m5: {a:d, b:d}
    let m5 = {};
    m5.a = 13.37;
    assertTrue(%HaveSameMap(m5, m2));
    m5.b = 13.37;
    assertFalse(%HaveSameMap(m5, m3));

    // At this point, Map3 and Map4 are both deprecated. Map2 transitions to
    // Map5. Map5 is the migration target for Map3.
    assertFalse(%HaveSameMap(m5, m3));

    // m6: {a:d, b:d, c:d}
    let m6 = {};
    m6.a = 13.37;
    assertTrue(%HaveSameMap(m6, m2));
    m6.b = 13.37;
    assertTrue(%HaveSameMap(m6, m5));
    m6.c = 13.37

    // Make m7: {a:d, b:d, c:t}
    let m7 = m4_2;
    assertTrue(%HaveSameMap(m7, m4));
    // Map4 is deprecated, so this property access triggers a Map migration.
    // With in-place map updates and no double unboxing, this should end up
    // migrating to Map6, and updating it in-place.
    m7.c;
    assertFalse(%HaveSameMap(m7, m4));
    assertTrue(%HaveSameMap(m6, m7));
})();
