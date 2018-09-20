// Copyright 2017 the V8 project authors. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
esid: ECMA-402 #sec-setnfdigitoptions
description: >
    The maximum and minimum fraction digits properties should be read from
    the options bag exactly once from the NumberFormat constructor.
    Regression test for https://bugs.chromium.org/p/v8/issues/detail?id=6015
include: [assert.js]
---*/

var minCounter = 0;
var maxCounter = 0;
new Intl.NumberFormat("en", { get minimumFractionDigits() { minCounter++ },
                              get maximumFractionDigits() { maxCounter++ } });
assert.sameValue(1, minCounter);
assert.sameValue(1, maxCounter);
