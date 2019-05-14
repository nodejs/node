// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Tests for primitive strings.

var iterator = 'ott'[Symbol.iterator]();

// These modifications shouldn't invalidate the String iterator protector.
iterator.__proto__.fonts = {};
assertTrue(%StringIteratorProtector());
iterator.__proto__[0] = 0;
assertTrue(%StringIteratorProtector());
