// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { Timeline } from "../../../tools/system-analyzer/timeline.mjs";
import { LogEntry} from "../../../tools/system-analyzer/log/log.mjs";


class TestLogEntry extends LogEntry {
  toString() {
    return `TestLogEntry(${this.id}, ${this.time})`;
  }
}

(function testTimeline() {
  let timeline = new Timeline();
  assertTrue(timeline.isEmpty());

  let id1 = "0x3e7e082470cd";
  let id2 = "0x3e7e082470ad";
  let time = 10;

  let entry1 = new TestLogEntry(id1, time + 0);
  let entry2 = new TestLogEntry(id1, time + 1);
  let entry3 = new TestLogEntry(id1, time + 2);
  let entry4 = new TestLogEntry(id1, time + 3);
  let entry5 = new TestLogEntry(id2, time + 3);
  let entry6 = new TestLogEntry(id1, time + 4);

  timeline.push(entry1);
  assertFalse(timeline.isEmpty());
  assertEquals(timeline.first(), entry1);
  assertEquals(timeline.at(0), entry1);
  assertEquals(timeline.last(), entry1);

  timeline.push(entry2);
  assertEquals(timeline.first(), entry1);
  assertEquals(timeline.at(1), entry2);
  assertEquals(timeline.last(), entry2);

  timeline.push(entry3);
  timeline.push(entry4);
  timeline.push(entry5);
  timeline.push(entry6);

  assertEquals(timeline.first(), entry1);
  assertEquals(timeline.at(0), entry1);
  assertEquals(timeline.at(1), entry2);
  assertEquals(timeline.at(2), entry3);
  assertEquals(timeline.at(3), entry4);
  assertEquals(timeline.at(4), entry5);
  assertEquals(timeline.at(5), entry6);
  assertEquals(timeline.last(), entry6);

  assertEquals(timeline.length, 6);
  assertEquals(timeline.all.length, 6);

  assertEquals(timeline.startTime, entry1.time);
  assertEquals(timeline.endTime, entry6.time);
  assertEquals(timeline.duration(), 4);

  assertEquals(timeline.findFirst(time - 1), 0);
  assertEquals(timeline.findFirst(time + 0), 0);
  assertEquals(timeline.findFirst(time + 1), 1);
  assertEquals(timeline.findFirst(time + 2), 2);
  assertEquals(timeline.findFirst(time + 3), 3);
  assertEquals(timeline.findFirst(time + 4), 5);
  assertEquals(timeline.findFirst(time + 5), 5);

  assertEquals(timeline.findFirst(time + 2.00), 2);
  assertEquals(timeline.findFirst(time + 2.01), 3);
  assertEquals(timeline.findFirst(time + 2.90), 3);
  assertEquals(timeline.findFirst(time + 3.01), 5);
  assertEquals(timeline.findFirst(time + 3.90), 5);
  assertEquals(timeline.findFirst(time + 4.00), 5);

  assertEquals(timeline.findLast(time - 1), -1);
  assertEquals(timeline.findLast(time + 0), 0);
  assertEquals(timeline.findLast(time + 1), 1);
  assertEquals(timeline.findLast(time + 2), 2);
  assertEquals(timeline.findLast(time + 3), 4);
  assertEquals(timeline.findLast(time + 4), 5);
  assertEquals(timeline.findLast(time + 5), 5);

  assertEquals(timeline.findLast(time + 2.00), 2);
  assertEquals(timeline.findLast(time + 2.01), 2);
  assertEquals(timeline.findLast(time + 2.90), 2);
  assertEquals(timeline.findLast(time + 3.01), 4);
  assertEquals(timeline.findLast(time + 3.90), 4);
  assertEquals(timeline.findLast(time + 4.00), 5);

  let startTime = time;
  let endTime = time + 2;
  timeline.selectTimeRange(startTime, endTime);
  assertArrayEquals(timeline.selection.startTime, startTime);
  assertArrayEquals(timeline.selection.endTime, endTime);
  assertArrayEquals(timeline.selection.values, [entry1, entry2]);
  let entryIdx = timeline.find(time + 1);
  let entry = timeline.at(entryIdx);
  assertEquals(entry.time, time + 1);
})();


(function testChunks() {
  const id1 = "0x3e7e082470cd";
  const time = 10;

  const entry1 = new TestLogEntry(id1, time + 0);
  const entry2 = new TestLogEntry(id1, time + 1);
  const entry3 = new TestLogEntry(id1, time + 2);
  const entry4 = new TestLogEntry(id1, time + 3);
  const entries = [entry1, entry2, entry3, entry4];

  const timeline1 = new Timeline(TestLogEntry, entries);
  assertEquals(timeline1.length, 4);
  assertEquals(timeline1.startTime, time);
  assertEquals(timeline1.endTime, time + 3);

  const chunks1_1 = timeline1.chunks(1);
  assertEquals(chunks1_1.length, 1);
  assertEquals(chunks1_1[0].start, timeline1.startTime);
  assertEquals(chunks1_1[0].end, timeline1.endTime);
  assertArrayEquals(chunks1_1[0].items, entries);

  const chunks1_4 = timeline1.chunks(4);
  assertEquals(chunks1_4.length, 4);
  assertEquals(chunks1_4[0].start, timeline1.startTime);
  assertArrayEquals(chunks1_4[0].items, [entry1]);
  assertEquals(chunks1_4[1].start, 10.75);
  assertArrayEquals(chunks1_4[1].items, [entry2]);
  assertEquals(chunks1_4[2].start, 11.5);
  assertArrayEquals(chunks1_4[2].items, [entry3]);
  assertEquals(chunks1_4[3].end, timeline1.endTime);
  assertArrayEquals(chunks1_4[3].items, [entry4]);

  const timeline2 = new Timeline(TestLogEntry, entries, 0, 100);
  assertEquals(timeline2.length, 4);
  assertEquals(timeline2.startTime, 0);
  assertEquals(timeline2.endTime, 100);
  assertEquals(timeline2.duration(), 100);

  const chunks2_1 = timeline2.chunks(1);
  assertEquals(chunks2_1.length, 1);
  assertEquals(chunks2_1[0].start, timeline2.startTime);
  assertEquals(chunks2_1[0].end, timeline2.endTime);
  assertArrayEquals(chunks1_1[0].items, entries);

  const chunks2_2 = timeline2.chunks(2);
  assertEquals(chunks2_2.length, 2);
  assertEquals(chunks2_2[0].start, 0);
  assertEquals(chunks2_2[0].end, 50);
  assertArrayEquals(chunks2_2[0].items, entries);
  assertEquals(chunks2_2[1].start, 50);
  assertEquals(chunks2_2[1].end, 100);
  assertArrayEquals(chunks2_2[1].items, []);

  const chunks2_4 = timeline2.chunks(4);
  assertEquals(chunks2_4.length, 4);
  assertEquals(chunks2_4[0].start, 0);
  assertEquals(chunks2_4[0].end, 25);
  assertArrayEquals(chunks2_4[0].items, entries);
  assertEquals(chunks2_4[1].start, 25);
  assertEquals(chunks2_4[1].end, 50);
  assertArrayEquals(chunks2_4[1].items, []);
  assertEquals(chunks2_4[2].start, 50);
  assertEquals(chunks2_4[2].end, 75);
  assertArrayEquals(chunks2_4[2].items, []);
  assertEquals(chunks2_4[3].start, 75);
  assertEquals(chunks2_4[3].end, 100);
  assertArrayEquals(chunks2_4[3].items, []);
})();