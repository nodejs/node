// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

({})['foobar\u2653'.slice(0, 6)] = null;
var x;
eval('x = function foobar() { return foobar };');
x();
