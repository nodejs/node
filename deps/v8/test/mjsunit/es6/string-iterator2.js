// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Tests for spreading primitive strings.

assertEquals([...''], []);

var str = 'ott';
assertEquals(['o', 't', 't'], [...str]);
assertTrue(%StringIteratorProtector());

str[Symbol.iterator] = {};
// Symbol.iterator can't be set on primitive strings, so it shouldn't invalidate
// the protector.
assertTrue(%StringIteratorProtector());

// This changes the String Iterator prototype. No more tests should be run after
// this in the same instance.
var iterator = str[Symbol.iterator]();
iterator.__proto__.next = () => ({value : undefined, done : true});

assertFalse(%StringIteratorProtector());
assertEquals([], [...str]);
