// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertEquals("a", RegExp.$1);
assertEquals("b", RegExp.$2);
assertEquals("c", RegExp.$3);
assertEquals("d", RegExp.$4);
assertEquals("e", RegExp.$5);
assertEquals("f", RegExp.$6);
assertEquals("g", RegExp.$7);
assertEquals("h", RegExp.$8);
assertEquals("i", RegExp.$9);

assertEquals("abcdefghij", RegExp.lastMatch);
assertEquals("j", RegExp.lastParen);
assertEquals(">>>", RegExp.leftContext);
assertEquals("<<<", RegExp.rightContext);
assertEquals(">>>abcdefghij<<<", RegExp.input);
