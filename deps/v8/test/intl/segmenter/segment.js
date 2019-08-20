// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-segmenter

assertEquals("function", typeof Intl.Segmenter.prototype.segment);
assertEquals(1, Intl.Segmenter.prototype.segment.length);

let seg = new Intl.Segmenter("en", {granularity: "word"})
let res;

// test with 0 args
assertDoesNotThrow(() => res = seg.segment())
// test with 1 arg
assertDoesNotThrow(() => res = seg.segment("hello"))
assertEquals("hello", res.next().value.segment);
// test with 2 args
assertDoesNotThrow(() => res = seg.segment("hello world"))
assertEquals("hello", res.next().value.segment);

// test with other types
assertDoesNotThrow(() => res = seg.segment(undefined))
assertEquals("undefined", res.next().value.segment);
assertDoesNotThrow(() => res = seg.segment(null))
assertEquals("null", res.next().value.segment);
assertDoesNotThrow(() => res = seg.segment(true))
assertEquals("true", res.next().value.segment);
assertDoesNotThrow(() => res = seg.segment(false))
assertEquals("false", res.next().value.segment);
assertDoesNotThrow(() => res = seg.segment(1234))
assertEquals("1234", res.next().value.segment);
assertDoesNotThrow(() => res = seg.segment(3.1415926))
assertEquals("3.1415926", res.next().value.segment);
assertDoesNotThrow(() => res = seg.segment(["hello","world"]))
assertEquals("hello", res.next().value.segment);
assertDoesNotThrow(() => res = seg.segment({k: 'v'}))
assertEquals("[", res.next().value.segment);
assertThrows(() => res = seg.segment(Symbol()), TypeError)
