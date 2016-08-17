// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Before Unicode RegExps are shipped, we shouldn't have the 'unicode'
// property on RegExp.prototype, or read it from 'flags'.
// mjsunit/es6/regexp-flags tests that the property is there when the
// flag is on.

// Flags: --no-harmony-unicode-regexps

'use strict';

assertFalse(RegExp.prototype.hasOwnProperty('unicode'));

// If we were going to be really strict, we could have a test like this,
// with the assertTrue replaced by assertFalse, since flags shouldn't
// Get the 'unicode' property. However, it is probably OK to omit this
// detailed fix.
var x = /a/;
var y = false;
Object.defineProperty(x, 'unicode', { get() { y = true; } });
assertEquals("", x.flags);
assertTrue(y);
