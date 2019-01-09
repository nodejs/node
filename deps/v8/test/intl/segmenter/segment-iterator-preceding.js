// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-segmenter

const segmenter = new Intl.Segmenter();
const text = "Hello World, Test 123! Foo Bar. How are you?";
const iter = segmenter.segment(text);

assertEquals("function", typeof iter.preceding);

// ToNumber("ABC") return NaN, ToInteger("ABC") return +0, ToIndex("ABC") return 0
assertThrows(() => iter.preceding("ABC"), RangeError);
// ToNumber(null) return +0, ToInteger(null) return +0, ToIndex(null) return 0
assertThrows(() => iter.preceding(null), RangeError);
assertThrows(() => iter.preceding(-3), RangeError);

// ToNumber(1.4) return 1.4, ToInteger(1.4) return 1, ToIndex(1.4) return 1
assertDoesNotThrow(() => iter.preceding(1.4));

// 1.5.3.3 %SegmentIteratorPrototype%.preceding( [ from ] )
// 3.b If ... from = 0, throw a RangeError exception.
assertThrows(() => iter.preceding(0), RangeError);

// 1.5.3.3 %SegmentIteratorPrototype%.preceding( [ from ] )
// 3.b If from > iterator.[[SegmentIteratorString]] ... , throw a RangeError exception.
assertDoesNotThrow(() => iter.preceding(text.length - 1));
assertDoesNotThrow(() => iter.preceding(text.length));
assertThrows(() => iter.preceding(text.length + 1), RangeError);
