// Copyright 2017 the V8 project authors. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
esid: ECMA-402 #sec-setnfdigitoptions
description: >
    When a currency is used in Intl.NumberFormat and minimumFractionDigits is
    not provided, maximumFractionDigits should be range-checked against it.
include: [assert.js]
---*/

assert.throws(RangeError,
              () => new Intl.NumberFormat('en', {
                  style: 'currency',
                  currency: 'USD',
                  maximumFractionDigits: 1
              }));
