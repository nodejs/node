// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function testBasicMark() {
  const mark = performance.mark("a mark");
  assertEquals("mark", mark.entryType);
  assertEquals("a mark", mark.name);
  assertTrue(typeof mark.startTime == "number");
  assertEquals(0, mark.duration);
})();


(function testMeasure1String() {
  const measure = performance.measure("a measure")
  assertEquals("measure", measure.entryType);
  assertEquals("a measure", measure.name);
  assertEquals(0, measure.startTime);
  assertTrue(typeof measure.duration == "number");
  assertTrue(0 <= measure.duration);
})();


(function testMarkAndMeasureLegacy() {
  const mark = performance.mark("a");
  const range_measure = performance.measure("a range measure", mark)
  assertEquals("measure", range_measure.entryType);
  assertEquals("a range measure", range_measure.name);
  assertEquals(mark.startTime, range_measure.startTime);
  assertTrue(typeof range_measure.duration == "number");
  assertTrue(0 <= range_measure.duration);
})();

(function testMeasureUnknownMark() {
  assertThrows(() => {
    performance.measure("a", "b");
  });
})();

(function testsMarkAndMeasure2Strings() {
  const mark = performance.mark("a");
  const measure = performance.measure("measure", "a");
  assertEquals(mark.startTime, measure.startTime)
})();

(function testsMarkAndMeasure3Strings() {
  const markA = performance.mark("a");
  const markB = performance.mark("b");
  const measure = performance.measure("measure", "a", "b");
  assertEquals(markA.startTime, measure.startTime);
  assertEquals(measure.duration, markB.startTime - markA.startTime);
})();
