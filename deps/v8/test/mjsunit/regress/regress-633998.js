// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var err_str_1 = "apply was called on , which is a object and not a function";
var err_str_2 =
  "apply was called on Error, which is a object and not a function";

var reached = false;
var error = new Error();
error.name = error;
try {
  Reflect.apply(error);
  reached = true;
} catch (e) {
  assertTrue(e.stack.indexOf(err_str_1) != -1);
} finally {
  assertFalse(reached);
}

reached = false;
error = new Error();
error.msg = error;
try {
  Reflect.apply(error);
  reached = true;
} catch (e) {
  assertTrue(e.stack.indexOf(err_str_2) != -1);
} finally {
  assertFalse(reached);
}

reached = false;
error = new Error();
error.name = error;
error.msg = error;
try {
  Reflect.apply(error);
  reached = true;
} catch (e) {
  assertTrue(e.stack.indexOf(err_str_1) != -1);
} finally {
  assertFalse(reached);
}
