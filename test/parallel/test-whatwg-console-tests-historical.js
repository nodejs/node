'use strict';

require('../common');

const { test, assert_equals } =
  require('../common/wpt');

/* eslint-disable max-len, quotes */

/* The following tests should not be modified as they are copied */
/* WPT Refs:
   https://github.com/web-platform-tests/wpt/blob/6f0a96ed650935b17b6e5d277889cfbe0ccc103e/console/console-tests-historical.any.js
   License: https://github.com/web-platform-tests/wpt/blob/6f0a96ed650935b17b6e5d277889cfbe0ccc103e/LICENSE.md
*/

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

/* eslint-enable */
