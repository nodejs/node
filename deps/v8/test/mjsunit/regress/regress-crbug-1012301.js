// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function f(o) {
  // The spread after the CloneObject IC shouldn't crash when trying to write a
  // double value to a field created by CloneObject.
  return {...o, ...{a:1.4}};
}

%EnsureFeedbackVectorForFunction(f);

var o = {};
// Train the CloneObject IC with a Double field.
o.a = 1.5;
f(o);
f(o);
f(o);
// Change the source map to have a Tagged field.
o.a = undefined;
f(o);
