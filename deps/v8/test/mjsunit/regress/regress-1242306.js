// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --sparkplug

function foo(){
  // __proto__ is a setter that is defined to return undefined.
  return __proto__ = 5;
}
assertEquals(foo(), 5);
assertEquals(foo(), 5);

%EnsureFeedbackVectorForFunction(foo);
assertEquals(foo(), 5);
assertEquals(foo(), 5);

%CompileBaseline(foo);
assertEquals(foo(), 5);
assertEquals(foo(), 5);
