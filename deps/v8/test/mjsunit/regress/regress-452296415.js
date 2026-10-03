// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
function makeJsonAndSetupMaps() {
  let o1 = { str: "A" };
  const length = 32;
  let o2 = { str: "B".repeat(length) };
  o2.f = {};
  let arr = [o1, o2];
  JSON.stringify(arr);
  return o1;
}

const feedback_obj = makeJsonAndSetupMaps();
const length = 32;
const long_str = "C".repeat(length);
const json_prefix = '[{"str":"A"}, {"str":"' + long_str + '", ';
const json_suffix = ']]';
const json = json_prefix + json_suffix;

// We want GC to happen during ReportUnexpectedToken when the syntax error is
// encountered.
%SetAllocationTimeout(1, 18);

assertThrows(() => JSON.parse(json));
