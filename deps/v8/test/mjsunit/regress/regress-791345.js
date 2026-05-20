// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(a) {
    var len = 0x80000000;
    arguments.length = len;
    Array.prototype.slice.call(arguments, len - 1, len);
}('a'));

(function(a) {
    var len = 0x40000000;
    arguments.length = len;
    Array.prototype.slice.call(arguments, len - 1, len);
}('a'));
