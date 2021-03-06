/**
 * These tests assert the non-existence of certain
 * legacy Console methods that are not included in
 * the specification: http://console.spec.whatwg.org/
 */

"use strict";

test(() => {
  assert_equals(console.timeline, undefined, "console.timeline should be undefined");
}, "'timeline' function should not exist on the console object");

test(() => {
  assert_equals(console.timelineEnd, undefined, "console.timelineEnd should be undefined");
}, "'timelineEnd' function should not exist on the console object");

test(() => {
  assert_equals(console.markTimeline, undefined, "console.markTimeline should be undefined");
}, "'markTimeline' function should not exist on the console object");
