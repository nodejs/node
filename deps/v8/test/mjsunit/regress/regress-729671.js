// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var o = { 0: 11, 1: 9};
assertThrows(() => JSON.parse('[0,0]', function() { this[1] = o; }), RangeError);
