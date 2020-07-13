// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var v = {};
Object.defineProperty(v.__proto__, "calendar",
    { get: function() { return -1; } });
assertThrows(() => new Intl.DateTimeFormat(v, 0), RangeError);
