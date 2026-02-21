// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var __v_2 = function() {};
var __v_3 = new Proxy({}, __v_2);
__v_2.__defineGetter__('name', function() {});
__v_2.get = function() { return "value 2" };
assertEquals(__v_3.property, "value 2");
