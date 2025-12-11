// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function g(s, key) { return s[key]; }

assertEquals(g(new String("a"), "length"), 1);
assertEquals(g(new String("a"), "length"), 1);
assertEquals(g("a", 32), undefined);
assertEquals(g("a", "length"), 1);
assertEquals(g(new String("a"), "length"), 1);
