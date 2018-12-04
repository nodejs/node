// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-segmenter

const segmenter = new Intl.Segmenter();
const text = "Hello World, Test 123! Foo Bar. How are you?";
const iter = segmenter.segment(text);

assertEquals("function", typeof iter.following);

// 1.5.3.2 %SegmentIteratorPrototype%.following( [ from ] )
// 3.b If from >= iterator.[[SegmentIteratorString]], throw a RangeError exception.
assertDoesNotThrow(() => iter.following(text.length - 1));
assertThrows(() => iter.following(text.length), RangeError);
assertThrows(() => iter.following(text.length + 1), RangeError);
