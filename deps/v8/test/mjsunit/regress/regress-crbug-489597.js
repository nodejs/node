// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

try {
  d8.file.execute("test/mjsunit/regress/regress-crbug-489597.js-script");
} catch (e) {
}

var o = this;
Error.captureStackTrace(o);
o.stack;
