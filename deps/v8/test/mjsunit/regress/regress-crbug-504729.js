// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (this.Worker) {
  Function.prototype.toString = "foo";
  function __f_7() {}
  assertThrows(function() { var __v_5 = new Worker(__f_7.toString(), {type: 'string'}) });
}
