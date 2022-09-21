// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

function getobj() {
  return { bar : function() { return 0}};
}

function foo() {
  var obj = getobj();
  var length = arguments.length;
  if (length == 0) {
     obj.bar();
  } else {
     obj.bar.apply(obj, arguments);
  }
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
assertOptimized(foo);
foo(10);
assertUnoptimized(foo);
%ClearFunctionFeedback(foo);
