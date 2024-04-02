// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var args = (function(a, b, c) { return arguments; })(1,2,3);
args[Symbol.isConcatSpreadable] = true;
assertEquals([1, 2, 3, 1, 2, 3], [].concat(args, args));

Object.defineProperty(args, "length", { value: 6 });
assertEquals([1, 2, 3, void 0, void 0, void 0], [].concat(args));
