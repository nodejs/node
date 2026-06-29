// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows("/[\\p]/u");
assertThrows("/[\\p{garbage}]/u");
assertThrows("/[\\p{}]/u");
assertThrows("/[\\p{]/u");
assertThrows("/[\\p}]/u");
assertThrows("/^[\\p{Lu}-\\p{Ll}]+$/u");

assertTrue(/^[\p{Lu}\p{Ll}]+$/u.test("ABCabc"));
assertTrue(/^[\p{Lu}-]+$/u.test("ABC-"));
assertFalse(/^[\P{Lu}\p{Ll}]+$/u.test("ABCabc"));
assertTrue(/^[\P{Lu}\p{Ll}]+$/u.test("abc"));
assertTrue(/^[\P{Lu}]+$/u.test("abc123"));
assertFalse(/^[\P{Lu}]+$/u.test("XYZ"));
assertTrue(/[\p{Math}]/u.test("+"));
assertTrue(/[\P{Bidi_M}]/u.test(" "));
assertTrue(/[\p{Hex}]/u.test("A"));

assertTrue(/^[^\P{Lu}]+$/u.test("XYZ"));
assertFalse(/^[^\p{Lu}\p{Ll}]+$/u.test("abc"));
assertFalse(/^[^\p{Lu}\p{Ll}]+$/u.test("ABC"));
assertTrue(/^[^\p{Lu}\p{Ll}]+$/u.test("123"));
assertTrue(/^[^\p{Lu}\P{Ll}]+$/u.test("abc"));
