// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Incomplete JSON, JSON.parse() will throw an exception when handling `"B`.
const jsonStr = '[{"A": 0},{"A": 0,"B';

// Prepare map transition: MapA --add-property(B)--> MapB.
// MapA and MapB shared same descriptors.
let o1 = {A: 0};
let o2 = {A: 0};
o2.B = 1;

// Clear reference, no objects use MapB, so only one entry in MapA->descriptors
// is actually used, and it will shrink during GC.
o2 = null;

// Trigger GC to shrink descriptors array during `ReportUnexpectedToken()`.
%SetAllocationTimeout(1, 50);

//JSON.parse(jsonStr);
assertThrows('JSON.parse(jsonStr)');
