// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

[1].toLocaleString();
delete Number.prototype.toLocaleString;
[1].toLocaleString();
var o = {};
o.__proto__ = { toString: Array.prototype.toString };
o.toString();
Number.prototype.arrayToString = Array.prototype.toString;
(42).arrayToString();
var a = [7];
a.toLocaleString();
