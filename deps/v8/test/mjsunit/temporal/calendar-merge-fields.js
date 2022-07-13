// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

// https://tc39.es/proposal-temporal/#sec-temporal.calendar.prototype.mergefields
let cal = new Temporal.Calendar("iso8601")

// Test throwing
assertThrows(() => cal.mergeFields(), TypeError);
assertThrows(() => cal.mergeFields(undefined, {}), TypeError);
assertThrows(() => cal.mergeFields(null, {}), TypeError);
assertThrows(() => cal.mergeFields({}, undefined), TypeError);
assertThrows(() => cal.mergeFields({}, null), TypeError);

// Test String, number, true, false, NaN, BigInt, Symbol types
// pending on https://github.com/tc39/proposal-temporal/issues/1647

// Assert only string will be merged
assertArrayEquals({}, cal.mergeFields({1: 2}, {3: 4}));
assertArrayEquals({}, cal.mergeFields({true: 2}, {false: 4}));
assertArrayEquals({}, cal.mergeFields({1n: 2}, {2n: 4}));
assertArrayEquals({}, cal.mergeFields({Infinity: 2}, {Infinity: 4}));
assertArrayEquals({}, cal.mergeFields({undefined: 2}, {NaN: 4}));
assertArrayEquals({}, cal.mergeFields({["foo"]: 2}, {["bar"]: 4}));
assertArrayEquals({a:1, b:2, c:4}, cal.mergeFields({a: 1, b: 2}, {b:3, c:4}));
assertArrayEquals({a:1, b:2, c:4, month:5},
    cal.mergeFields({a: 1, b: 2}, {b:3, c:4, month:5}));
assertArrayEquals({a:1, b:2, c:4, month:5, month:'M06'},
    cal.mergeFields({a: 1, b: 2}, {b:3, c:4, month:5, monthCode:'M06'}));
assertArrayEquals({a:1, b:2, c:4, month:'M06'}, cal.mergeFields({a: 1, b: 2},
      {b:3, c:4, monthCode:'M06'}));

assertArrayEquals({a:1, b:2, c:4, month:5},
    cal.mergeFields({a: 1, b: 2, month:7}, {b:3, c:4, month:5}));
assertArrayEquals({a:1, b:2, c:4, month:5},
    cal.mergeFields({a: 1, b: 2, month:7, monthCode:'M08'},
      {b:3, c:4, month:5}));
assertArrayEquals({a:1, b:2, c:4, monthCode:'M06'},
    cal.mergeFields({a: 1, b: 2, month:7}, {b:3, c:4, monthCode:'M06'}));
assertArrayEquals({a:1, b:2, c:4, monthCode:'M06'},
    cal.mergeFields({a: 1, b: 2, month:7, monthCode:'M08'},
      {b:3, c:4, monthCode:'M06'}));
assertArrayEquals({a:1, b:2, c:4, month:5, monthCode:'M06'},
    cal.mergeFields({a: 1, b: 2, month:7},
      {b:3, c:4, month:5, monthCode:'M06'}));
assertArrayEquals({a:1, b:2, c:4, month:5, monthCode:'M06'},
    cal.mergeFields({a: 1, b: 2, month:7, monthCode:'M08'},
      {b:3, c:4, month:5, monthCode:'M06'}));

assertArrayEquals({a:1, b:2, c:4, month:7},
    cal.mergeFields({a: 1, b: 2, month:7}, {b:3, c:4}));
assertArrayEquals({a:1, b:2, c:4, month:5, monthCode:'M08'},
    cal.mergeFields({a: 1, b: 2, month:7, monthCode:'M08'}, {b:3, c:4}));
assertArrayEquals({a:1, b:2, c:4, month:7, monthCode:'M08'},
    cal.mergeFields({a: 1, b: 2, month:7, monthCode:'M08'}, {b:3, c:4}));
assertArrayEquals({a:1, b:2, c:4, monthCode:'M08'},
    cal.mergeFields({a: 1, b: 2, monthCode:'M08'}, {b:3, c:4}));
