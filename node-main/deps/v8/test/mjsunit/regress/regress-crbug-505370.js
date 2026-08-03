// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var o = {
  get 0() { reference_error;  },
  get length() { return 1; }
};

var method_name;

try {
  o[0];
} catch (e) {
  thrown = true;
  Error.prepareStackTrace = function(exception, frames) { return frames; };
  var frames = e.stack;
  Error.prepareStackTrace = undefined;
  method_name = frames[0].getMethodName();
}

assertEquals("0", method_name);
