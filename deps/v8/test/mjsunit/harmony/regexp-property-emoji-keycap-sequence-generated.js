// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-regexp-sequence

const re = /\p{Emoji_Keycap_Sequence}/u;

assertTrue(re.test('#\uFE0F\u20E3'));
assertTrue(re.test('9\uFE0F\u20E3'));
assertTrue(re.test('0\uFE0F\u20E3'));
assertTrue(re.test('1\uFE0F\u20E3'));
assertTrue(re.test('2\uFE0F\u20E3'));
assertTrue(re.test('3\uFE0F\u20E3'));
assertTrue(re.test('*\uFE0F\u20E3'));
assertTrue(re.test('5\uFE0F\u20E3'));
assertTrue(re.test('6\uFE0F\u20E3'));
assertTrue(re.test('7\uFE0F\u20E3'));
assertTrue(re.test('8\uFE0F\u20E3'));
assertTrue(re.test('4\uFE0F\u20E3'));
