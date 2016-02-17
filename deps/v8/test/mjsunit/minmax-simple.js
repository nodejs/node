// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-natives-as natives
// Test the MaxSimple and MinSimple internal methods in runtime.js

var MaxSimple = natives.ImportNow("MaxSimple");
var MinSimple = natives.ImportNow("MinSimple");

function checkEvaluations(target) {
  var evaluations = 0;
  var observedNumber = {
    valueOf: function() {
      evaluations++;
      return 0;
    }
  };
  target(observedNumber, observedNumber);
  return evaluations;
}

assertEquals(1, MaxSimple(-1, 1));
assertEquals(2, checkEvaluations(MaxSimple));

assertEquals(-1, MinSimple(-1, 1));
assertEquals(2, checkEvaluations(MinSimple));
