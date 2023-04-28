// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var o1 = {};
o1.x = 1
o1.y = 1.5
var o2 = {}
o2.x = 1.5;
o2.__defineSetter__('y', function(v) { });
o1.y;
