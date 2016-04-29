// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// lastIndex is set according to funny rules. It is typically set only
// for global or sticky RegExps, but on a failure to find a match, it is
// set unconditionally. If a set fails, then it acts as if in strict mode
// and throws.

var re = /x/;
Object.defineProperty(re, 'lastIndex', {writable: false});
assertThrows(() => re.exec(""), TypeError);
assertEquals(["x"], re.exec("x"));

var re = /x/y;
Object.defineProperty(re, 'lastIndex', {writable: false});
assertThrows(() => re.exec(""), TypeError);
assertThrows(() => re.exec("x"), TypeError);
