// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const str = 'aaaa';

const re0 = /./y;

// Twice to go through both runtime and the builtin.
re0.lastIndex = 9;
assertEquals(str, re0[Symbol.replace](str, () => 42));
re0.lastIndex = 9;
assertEquals(str, re0[Symbol.replace](str, () => 42));
re0.lastIndex = 9;
assertEquals(str, re0[Symbol.replace](str, "42"));
re0.lastIndex = 9;
assertEquals(str, re0[Symbol.replace](str, "42"));
