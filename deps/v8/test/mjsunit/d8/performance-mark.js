// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const mark = performance.mark("a mark");
assertEquals("mark", mark.entryType);
assertEquals("a mark", mark.name);
assertTrue(typeof mark.startTime == "number");
assertEquals(0, mark.duration);

const measure = performance.measure("a measure")
assertEquals("measure", measure.entryType);
assertEquals("a measure", measure.name);
assertEquals(0, measure.startTime);
assertTrue(typeof mark.duration == "number");
assertTrue(mark.startTime <= measure.duration);

const range_measure = performance.measure("a range measure", mark)
assertEquals("measure", range_measure.entryType);
assertEquals("a range measure", range_measure.name);
assertEquals(mark.startTime, range_measure.startTime);
assertTrue(typeof range_measure.duration == "number");
assertTrue(0 <= range_measure.duration);
