// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var args = (function(a) { return arguments; })(1,2,3);
delete args[1];
args[Symbol.isConcatSpreadable] = true;
assertEquals([1, , 3, 1, , 3], [].concat(args, args));
