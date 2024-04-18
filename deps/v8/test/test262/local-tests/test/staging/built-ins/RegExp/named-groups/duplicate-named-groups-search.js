// Copyright (C) 2024 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Test search function with duplicate names in alteration.
features: [regexp-duplicate-named-groups]
includes: [compareArray.js]
---*/

assert.compareArray(3, 'abcxyz'.search(/(?<a>x)|(?<a>y)/));
assert.compareArray(3, 'abcxyz'.search(/(?<a>y)|(?<a>x)/));
assert.compareArray(1, 'aybcxyz'.search(/(?<a>x)|(?<a>y)/));
assert.compareArray(1, 'aybcxyz'.search(/(?<a>y)|(?<a>x)/));
