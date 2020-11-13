// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { Timeline } from "../../../tools/system-analyzer/timeline.mjs";
import { Event } from "../../../tools/system-analyzer/log/log.mjs";


(function testTimeline() {
  let timeline = new Timeline();
  let id1 = "0x3e7e082470cd";
  let id2 = "0x3e7e082470ad";
  let time = 12;
  let event1 = new Event(id1, time);
  let event2 = new Event(id1, time + 1);
  let event3 = new Event(id1, time + 2);
  let event4 = new Event(id1, time + 3);
  let event5 = new Event(id2, time + 3);
  timeline.push(event1);
  timeline.push(event2);
  timeline.push(event3);
  timeline.push(event4);
  timeline.push(event5);
  let startTime = time;
  let endTime = time + 2;
  timeline.selectTimeRange(startTime, endTime);
  assertArrayEquals(timeline.selection, [event1, event2, event3]);
  let entryIdx = timeline.find(time + 1);
  let entry = timeline.at(entryIdx);
  assertEquals(entry.time, time + 1);
})();
