// Copyright (C) 2024 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Test different functions with duplicate names in alteration.
features: [regexp-duplicate-named-groups]
includes: [compareArray.js]
---*/

assert.compareArray(
    ['xxyy', undefined, 'y'], /(?:(?:(?<a>x)|(?<a>y))\k<a>){2}/.exec('xxyy'));
assert.compareArray(
    ['zzyyxx', 'x', undefined, undefined, undefined, undefined],
    /(?:(?:(?<a>x)|(?<a>y)|(a)|(?<b>b)|(?<a>z))\k<a>){3}/.exec('xzzyyxxy'));
assert.compareArray(
    ['xxyy', undefined, 'y'], 'xxyy'.match(/(?:(?:(?<a>x)|(?<a>y))\k<a>){2}/));
