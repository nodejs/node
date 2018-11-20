// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function foo(){}
Object.defineProperty(foo, "prototype", { value: 2 });
assertEquals(2, foo.prototype);

function bar(){}
Object.defineProperty(bar, "prototype", { value: 2, writable: false });
assertEquals(2, bar.prototype);
assertThrows(function() { "use strict"; bar.prototype = 10; }, TypeError);
assertEquals(false, Object.getOwnPropertyDescriptor(bar,"prototype").writable);
