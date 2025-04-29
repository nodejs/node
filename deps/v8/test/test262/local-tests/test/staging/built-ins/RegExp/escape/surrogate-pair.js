// Copyright (C) 2025 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Unescaped surrogate pairs
features: [RegExp.escape]
---*/

assert.sameValue(RegExp.escape("\ud800\udc00"), "\ud800\udc00", 'Unescaped surrogate pair');
