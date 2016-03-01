// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Error.prepareStackTrace = (e,s) => s;
var __v_3 = Error().stack[0].constructor;
var __v_4 = {};
function __f_3() {}
var __v_5 = __v_3.call(null, __v_4, __f_3, {valueOf() { return 1611877293 }});
 __v_5.getColumnNumber();
