// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// In ES#sec-array.prototype.concat
// When concat makes a new integer-indexed exotic object, the resulting properties
// are non-configurable and cannot have CreateDataPropertyOrThrow called on them,
// so it throws a TypeError on failure to make a new property.

__v_0 = new Uint8Array(100);
array = new Array(10);
array.__proto__ = __v_0;
assertThrows(() => Array.prototype.concat.call(array), TypeError);
