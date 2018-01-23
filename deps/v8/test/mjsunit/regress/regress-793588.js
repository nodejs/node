// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-regexp-property

assertNull(/a\P{Any}a/u.exec("a\u{d83d}a"));
assertEquals(["a\u{d83d}a"], /a\p{Any}a/u.exec("a\u{d83d}a"));
assertEquals(["a\u{d83d}a"], /(?:a\P{Any}a|a\p{Any}a)/u.exec("a\u{d83d}a"));
assertNull(/a[\P{Any}]a/u.exec("a\u{d83d}a"));
assertEquals(["a\u{d83d}a"], /a[^\P{Any}]a/u.exec("a\u{d83d}a"));
assertEquals(["a\u{d83d}a"], /a[^\P{Any}x]a/u.exec("a\u{d83d}a"));
assertNull(/a[^\P{Any}x]a/u.exec("axa"));
