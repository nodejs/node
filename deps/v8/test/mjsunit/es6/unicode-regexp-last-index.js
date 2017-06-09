// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-regexp-lookbehind

var r = /./ug;
assertEquals(["\ud800\udc00"], r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(2, r.lastIndex);
r.lastIndex = 1;
assertEquals(["\ud800\udc00"], r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(2, r.lastIndex);
assertEquals(["\ud801\udc01"], r.exec("\ud800\udc00\ud801\udc01"));
r.lastIndex = 3;
assertEquals(["\ud801\udc01"], r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(4, r.lastIndex);
r.lastIndex = 4;
assertNull(r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(0, r.lastIndex);
r.lastIndex = 5;
assertNull(r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(0, r.lastIndex);

r.lastIndex = 3;
assertEquals(["\ud802"], r.exec("\ud800\udc00\ud801\ud802"));
r.lastIndex = 4;
assertNull(r.exec("\ud800\udc00\ud801\ud802"));

r = /./g;
assertEquals(["\ud800"], r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(1, r.lastIndex);
assertEquals(["\udc00"], r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(2, r.lastIndex);
assertEquals(["\ud801"], r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(3, r.lastIndex);
assertEquals(["\udc01"], r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(4, r.lastIndex);
assertNull(r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(0, r.lastIndex);
r.lastIndex = 1;
assertEquals(["\udc00"], r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(2, r.lastIndex);

// ------------------------

r = /^./ug;
assertEquals(["\ud800\udc00"], r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(2, r.lastIndex);
r.lastIndex = 1;
assertEquals(["\ud800\udc00"], r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(2, r.lastIndex);
assertNull(r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(0, r.lastIndex);
r.lastIndex = 3;
assertNull(r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(0, r.lastIndex);
r.lastIndex = 4;
assertNull(r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(0, r.lastIndex);
r.lastIndex = 5;
assertNull(r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(0, r.lastIndex);

r = /^./g;
assertEquals(["\ud800"], r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(1, r.lastIndex);
assertNull(r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(0, r.lastIndex);
r.lastIndex = 3;
assertNull(r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(0, r.lastIndex);

//------------------------

r = /(?:(^.)|.)/ug;
assertEquals(["\ud800\udc00", "\ud800\udc00"],
             r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(2, r.lastIndex);
r.lastIndex = 1;
assertEquals(["\ud800\udc00", "\ud800\udc00"],
             r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(2, r.lastIndex);
assertEquals(["\ud801\udc01", undefined], r.exec("\ud800\udc00\ud801\udc01"));
r.lastIndex = 3;
assertEquals(["\ud801\udc01", undefined], r.exec("\ud800\udc00\ud801\udc01"));
r.lastIndex = 4;
assertNull(r.exec("\ud800\udc00\ud801\udc01"));
r.lastIndex = 5;
assertNull(r.exec("\ud800\udc00\ud801\udc01"));

r.lastIndex = 3;
assertEquals(["\ud802", undefined], r.exec("\ud800\udc00\ud801\ud802"));
r.lastIndex = 4;
assertNull(r.exec("\ud800\udc00\ud801\ud802"));

r = /(?:(^.)|.)/g;
assertEquals(["\ud800", "\ud800"],
    r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(1, r.lastIndex);
assertEquals(["\udc00", undefined], r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(2, r.lastIndex);
r.lastIndex = 3;
assertEquals(["\udc01", undefined], r.exec("\ud800\udc00\ud801\udc01"));
assertEquals(4, r.lastIndex);
