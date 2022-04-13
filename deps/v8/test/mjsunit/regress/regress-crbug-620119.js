// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy

assertEquals(0, ((x, {[(x = function() { y = 0 }, "foo")]: y = eval(1)}) => { x(); return y })(42, {}));
assertEquals(0, (function (x, {[(x = function() { y = 0 }, "foo")]: y = eval(1)}) { x(); return y })(42, {}));
