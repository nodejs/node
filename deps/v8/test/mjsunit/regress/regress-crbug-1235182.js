// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var call_f = new Function('f(' + ('0,').repeat(7023) + ')');
function f() {[1, 2, 3].sort(call_f);}
assertThrows(call_f, RangeError);
