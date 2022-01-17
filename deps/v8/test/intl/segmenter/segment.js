// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertEquals("function", typeof Intl.Segmenter.prototype.segment);
assertEquals(1, Intl.Segmenter.prototype.segment.length);

let seg = new Intl.Segmenter("en", {granularity: "word"})
let segments;

// test with 0 args
assertDoesNotThrow(() => segments = seg.segment())
// test with 1 arg
assertDoesNotThrow(() => segments = seg.segment("hello"))
assertEquals("hello", segments.containing(0).input);
// test with 2 args
assertDoesNotThrow(() => segments = seg.segment("hello world"))
assertEquals("hello world", segments.containing(0).input);

// test with other types
assertDoesNotThrow(() => segments = seg.segment(undefined))
assertEquals("undefined", segments.containing(0).input);
assertDoesNotThrow(() => segments = seg.segment(null))
assertEquals("null", segments.containing(0).input);
assertDoesNotThrow(() => segments = seg.segment(true))
assertEquals("true", segments.containing(0).input);
assertDoesNotThrow(() => segments = seg.segment(false))
assertEquals("false", segments.containing(0).input);
assertDoesNotThrow(() => segments = seg.segment(1234))
assertEquals("1234", segments.containing(0).input);
assertDoesNotThrow(() => segments = seg.segment(3.1415926))
assertEquals("3.1415926", segments.containing(0).input);
assertDoesNotThrow(() => segments = seg.segment(98765432109876543210987654321n))
assertEquals("98765432109876543210987654321", segments.containing(0).input);
assertDoesNotThrow(() => segments = seg.segment(["hello","world"]))
assertEquals("hello,world", segments.containing(0).input);
assertDoesNotThrow(() => segments = seg.segment({k: 'v'}))
assertEquals("[object Object]", segments.containing(0).input);
assertThrows(() => segments = seg.segment(Symbol()), TypeError)
