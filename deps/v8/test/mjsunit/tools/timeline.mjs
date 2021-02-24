// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { Timeline } from "../../../tools/system-analyzer/timeline.mjs";
import { LogEntry} from "../../../tools/system-analyzer/log/log.mjs";


(function testTimeline() {
  let timeline = new Timeline();

  let id1 = "0x3e7e082470cd";
  let id2 = "0x3e7e082470ad";
  let time = 10;

  let entry1 = new LogEntry(id1, time + 0);
  let entry2 = new LogEntry(id1, time + 1);
  let entry3 = new LogEntry(id1, time + 2);
  let entry4 = new LogEntry(id1, time + 3);
  let entry5 = new LogEntry(id2, time + 3);
  let entry6 = new LogEntry(id1, time + 4);

  timeline.push(entry1);
  timeline.push(entry2);
  timeline.push(entry3);
  timeline.push(entry4);
  timeline.push(entry5);
  timeline.push(entry6);

  assertEquals(timeline.find(time + 0), 0);
  assertEquals(timeline.find(time + 1), 1);
  assertEquals(timeline.find(time + 2), 2);
  assertEquals(timeline.find(time + 3), 3);
  assertEquals(timeline.find(time + 4), 5);
  assertEquals(timeline.find(time + 5), 5);

  assertEquals(timeline.find(time + 2.00), 2);
  assertEquals(timeline.find(time + 2.01), 3);
  assertEquals(timeline.find(time + 2.90), 3);
  assertEquals(timeline.find(time + 3.01), 5);
  assertEquals(timeline.find(time + 3.90), 5);
  assertEquals(timeline.find(time + 4.00), 5);

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
