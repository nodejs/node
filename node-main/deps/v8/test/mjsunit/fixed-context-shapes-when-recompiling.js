// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --lazy-inner-functions

// Test that the information on which variables to allocate in context doesn't
// change when recompiling.

function TestVarInInnerFunction() {
  // Introduce variables which would potentially be context allocated, depending
  // on whether an inner function refers to them or not.
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    var a; // This will make "a" actually not be context allocated.
    a; b; c;
  }
  // Force recompilation.
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestVarInInnerFunction);
TestVarInInnerFunction();


// Other tests are the same, except that the shadowing variable "a" in inner
// functions is declared differently.

function TestLetInInnerFunction() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    let a;
    a; b; c;
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestLetInInnerFunction);
TestLetInInnerFunction();

function TestConstInInnerFunction() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    const a = 0;
    a; b; c;
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestConstInInnerFunction);
TestConstInInnerFunction();

function TestInnerFunctionParameter() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner(a) {
    a; b; c;
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerFunctionParameter);
TestInnerFunctionParameter();

function TestInnerFunctionRestParameter() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner(...a) {
    a; b; c;
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerFunctionRestParameter);
TestInnerFunctionRestParameter();

function TestInnerFunctionDestructuredParameter_1() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner([d, a]) {
    a; b; c;
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerFunctionDestructuredParameter_1);
TestInnerFunctionDestructuredParameter_1();

function TestInnerFunctionDestructuredParameter_2() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner({d, a}) {
    a; b; c;
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerFunctionDestructuredParameter_2);
TestInnerFunctionDestructuredParameter_2();

function TestInnerArrowFunctionParameter() {
  var a = 1;
  var b = 2;
  var c = 3;
  (a) => { a; b; c; }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerArrowFunctionParameter);
TestInnerArrowFunctionParameter();

function TestInnerArrowFunctionRestParameter() {
  var a = 1;
  var b = 2;
  var c = 3;
  (...a) => { a; b; c; }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerArrowFunctionRestParameter);
TestInnerArrowFunctionRestParameter();

function TestInnerArrowFunctionDestructuredParameter_1() {
  var a = 1;
  var b = 2;
  var c = 3;
  ([d, a]) => { a; b; c; }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerArrowFunctionDestructuredParameter_1);
TestInnerArrowFunctionDestructuredParameter_1();

function TestInnerArrowFunctionDestructuredParameter_2() {
  var a = 1;
  var b = 2;
  var c = 3;
  ({d, a}) => { a; b; c;  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerArrowFunctionDestructuredParameter_2);
TestInnerArrowFunctionDestructuredParameter_2();

function TestInnerInnerFunctionParameter() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    function innerinner(a) { a; b; c; }
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerInnerFunctionParameter);
TestInnerInnerFunctionParameter();

function TestInnerInnerFunctionRestParameter() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    function innerinner(...a) { a; b; c; }
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerInnerFunctionRestParameter);
TestInnerInnerFunctionRestParameter();

function TestInnerInnerFunctionDestructuredParameter_1() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    function innerinner({d, a}) { a; b; c; }
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerInnerFunctionDestructuredParameter_1);
TestInnerInnerFunctionDestructuredParameter_1();

function TestInnerInnerFunctionDestructuredParameter_2() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    function innerinner([d, a]) { a; b; c; }
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerInnerFunctionDestructuredParameter_2);
TestInnerInnerFunctionDestructuredParameter_2();

function TestInnerInnerArrowFunctionParameter() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    var f = a => a + b + c;
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerInnerArrowFunctionParameter);
TestInnerInnerArrowFunctionParameter();

function TestInnerInnerArrowFunctionRestParameter() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    var f = (...a) => a + b + c;
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerInnerArrowFunctionRestParameter);
TestInnerInnerArrowFunctionRestParameter();

function TestInnerInnerArrowFunctionDestructuredParameter_1() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    var f = ([d, a]) => a + b + c;
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerInnerArrowFunctionDestructuredParameter_1);
TestInnerInnerArrowFunctionDestructuredParameter_1();

function TestInnerInnerArrowFunctionDestructuredParameter_2() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    var f = ({d, a}) => a + b + c;
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerInnerArrowFunctionDestructuredParameter_2);
TestInnerInnerArrowFunctionDestructuredParameter_2();

function TestInnerFunctionInnerFunction() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    function a() { }
    a; b; c;
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerFunctionInnerFunction);
TestInnerFunctionInnerFunction();

function TestInnerFunctionSloppyBlockFunction() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    if (true) { function a() { } }
    a; b; c;
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerFunctionSloppyBlockFunction);
TestInnerFunctionSloppyBlockFunction();

function TestInnerFunctionCatchVariable() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    try {
    }
    catch(a) {
      a; b; c;
    }
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
};
%PrepareFunctionForOptimization(TestInnerFunctionCatchVariable);
TestInnerFunctionCatchVariable();

function TestInnerFunctionLoopVariable1() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    for (var a in {}) {
      a; b; c;
    }
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerFunctionLoopVariable1);
TestInnerFunctionLoopVariable1();

function TestInnerFunctionLoopVariable2() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    for (let a in {}) {
      a; b; c;
    }
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerFunctionLoopVariable2);
TestInnerFunctionLoopVariable2();

function TestInnerFunctionLoopVariable3() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    for (var a of []) {
      a; b; c;
    }
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerFunctionLoopVariable3);
TestInnerFunctionLoopVariable3();

function TestInnerFunctionLoopVariable4() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    for (let a of []) {
      a; b; c;
    }
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerFunctionLoopVariable4);
TestInnerFunctionLoopVariable4();

function TestInnerFunctionClass() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    class a {}
    a; b; c;
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerFunctionClass);
TestInnerFunctionClass();

function TestInnerFunctionDestructuring1() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    var [a, a2] = [1, 2];
    a; b; c;
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerFunctionDestructuring1);
TestInnerFunctionDestructuring1();

function TestInnerFunctionDestructuring2() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    let [a, a2] = [1, 2];
    a; b; c;
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerFunctionDestructuring2);
TestInnerFunctionDestructuring2();

function TestInnerFunctionDestructuring3() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    const [a, a2] = [1, 2];
    a; b; c;
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerFunctionDestructuring3);
TestInnerFunctionDestructuring3();

function TestInnerFunctionDestructuring4() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    var [a2, ...a] = [1, 2];
    a; b; c;
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerFunctionDestructuring4);
TestInnerFunctionDestructuring4();

function TestInnerFunctionDestructuring5() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    let [a2, ...a] = [1, 2];
    a; b; c;
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerFunctionDestructuring5);
TestInnerFunctionDestructuring5();

function TestInnerFunctionDestructuring6() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    const [a2, ...a] = [1, 2];
    a; b; c;
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerFunctionDestructuring6);
TestInnerFunctionDestructuring6();

function TestInnerFunctionDestructuring7() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    var {a, a2} = {a: 1, a2: 2};
    a; b; c;
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerFunctionDestructuring7);
TestInnerFunctionDestructuring7();

function TestInnerFunctionDestructuring8() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    let {a, a2} = {a: 1, a2: 2};
    a; b; c;
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerFunctionDestructuring8);
TestInnerFunctionDestructuring8();

function TestInnerFunctionDestructuring9() {
  var a = 1;
  var b = 2;
  var c = 3;
  function inner() {
    const {a, a2} = {a: 1, a2: 2};
    a; b; c;
  }
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(3, c);
  }
}
%PrepareFunctionForOptimization(TestInnerFunctionDestructuring9);
TestInnerFunctionDestructuring9();

// A cluster of similar tests where the inner function only declares a variable
// whose name clashes with an outer function variable name, but doesn't use it.
function TestRegress650969_1_var() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      var a;
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_1_var);
TestRegress650969_1_var();

function TestRegress650969_1_let() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      let a;
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_1_let);
TestRegress650969_1_let();

function TestRegress650969_2_var() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      var a = 6;
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_2_var);
TestRegress650969_2_var();

function TestRegress650969_2_let() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      let a = 6;
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_2_let);
TestRegress650969_2_let();

function TestRegress650969_2_const() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      const a = 6;
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_2_const);
TestRegress650969_2_const();

function TestRegress650969_3_var() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      var a, b;
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_3_var);
TestRegress650969_3_var();

function TestRegress650969_3_let() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      let a, b;
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_3_let);
TestRegress650969_3_let();

function TestRegress650969_4_var() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      var a = 6, b;
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_4_var);
TestRegress650969_4_var();

function TestRegress650969_4_let() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      let a = 6, b;
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_4_let);
TestRegress650969_4_let();

function TestRegress650969_4_const() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      const a = 0, b = 0;
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_4_const);
TestRegress650969_4_const();

function TestRegress650969_9_parameter() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner(a) {}
  }
}
%PrepareFunctionForOptimization(TestRegress650969_9_parameter);
TestRegress650969_9_parameter();

function TestRegress650969_9_restParameter() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner(...a) {}
  }
}
%PrepareFunctionForOptimization(TestRegress650969_9_restParameter);
TestRegress650969_9_restParameter();

function TestRegress650969_9_destructuredParameter_1() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner([d, a]) {}
  }
}
%PrepareFunctionForOptimization(TestRegress650969_9_destructuredParameter_1);
TestRegress650969_9_destructuredParameter_1();

function TestRegress650969_9_destructuredParameter_2() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner({d, a}) {}
  }
}
%PrepareFunctionForOptimization(TestRegress650969_9_destructuredParameter_2);
TestRegress650969_9_destructuredParameter_2();

function TestRegress650969_10_parameter() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      function innerinner(a) {}
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_10_parameter);
TestRegress650969_10_parameter();

function TestRegress650969_10_restParameter() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      function innerinner(...a) {}
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_10_restParameter);
TestRegress650969_10_restParameter();

function TestRegress650969_10_destructuredParameter_1() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      function innerinner([d, a]) {}
    }
  }
}
%PrepareFunctionForOptimization( TestRegress650969_10_destructuredParameter_1);
TestRegress650969_10_destructuredParameter_1();

function TestRegress650969_10_destructuredParameter_2() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      function innerinner({d, a}) {}
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_10_destructuredParameter_2);
TestRegress650969_10_destructuredParameter_2();

function TestRegress650969_11_var() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      var [a, b] = [1, 2];
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_11_var);
TestRegress650969_11_var();


function TestRegress650969_11_let() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      let [a, b] = [1, 2];
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_11_let);
TestRegress650969_11_let();

function TestRegress650969_11_const() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      const [a, b] = [1, 2];
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_11_const);
TestRegress650969_11_const();

function TestRegress650969_12_var() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      var [b, a] = [1, 2];
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_12_var);
TestRegress650969_12_var();

function TestRegress650969_12_let() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      let [b, a] = [1, 2];
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_12_let);
TestRegress650969_12_let();

function TestRegress650969_12_const() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      const [b, a] = [1, 2];
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_12_const);
TestRegress650969_12_const();

function TestRegress650969_13_var() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      var [b, ...a] = [1, 2];
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_13_var);
TestRegress650969_13_var();

function TestRegress650969_13_let() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      let [b, ...a] = [1, 2];
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_13_let);
TestRegress650969_13_let();

function TestRegress650969_13_const() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      const [b, ...a] = [1, 2];
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_13_const);
TestRegress650969_13_const();

function TestRegress650969_14_var() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      var {a, b} = {a: 1, b: 2};
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_14_var);
TestRegress650969_14_var();

function TestRegress650969_14_let() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      let {a, b} = {a: 1, b: 2};
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_14_let);
TestRegress650969_14_let();

function TestRegress650969_14_const() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      const {a, b} = {a: 1, b: 2};
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_14_const);
TestRegress650969_14_const();

function TestRegress650969_15_var() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      var {b: {a}, c} = {b: {a: 1}, c: 2};
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_15_var);
TestRegress650969_15_var();

function TestRegress650969_15_let() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      let {b: {a}, c} = {b: {a: 1}, c: 2};
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_15_let);
TestRegress650969_15_let();

function TestRegress650969_15_const() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      const {b: {a}, c} = {b: {a: 1}, c: 2};
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_15_const);
TestRegress650969_15_const();

function TestRegress650969_16_var() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      var {a: {b}, c} = {a: {b: 1}, c: 2};
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_16_var);
TestRegress650969_16_var();

function TestRegress650969_16_let() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      let {a: {b}, c} = {a: {b: 1}, c: 2};
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_16_let);
TestRegress650969_16_let();

function TestRegress650969_16_const() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      const {a: {b}, c} = {a: {b: 1}, c: 2};
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_16_const);
TestRegress650969_16_const();

function TestRegress650969_17_var() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      for (var a = 0; 0 == 1; ) { }
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_17_var);
TestRegress650969_17_var();

function TestRegress650969_17_let() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      for (let a = 0; 0 == 1; ) { }
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_17_let);
TestRegress650969_17_let();

function TestRegress650969_17_const() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      for (const a = 0; 0 == 1; ) { }
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_17_const);
TestRegress650969_17_const();

function TestRegress650969_18() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      function innerinner([a, b]) {}
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_18);
TestRegress650969_18();

function TestRegress650969_18() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a;
    function inner() {
      function innerinner(a) {}
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_18);
TestRegress650969_18();

// Regression tests for an intermediate stage where unresolved references were
// discarded too aggressively.
function TestRegress650969_sidetrack_var() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a = 0;
    function inner() {
      return a;
      var {b: {a}, c} = {b: {a: 1}, c: 2};
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_sidetrack_var);
TestRegress650969_sidetrack_var();

function TestRegress650969_sidetrack_let() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a = 0;
    function inner() {
      return a;
      let {b: {a}, c} = {b: {a: 1}, c: 2};
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_sidetrack_let);
TestRegress650969_sidetrack_let();

function TestRegress650969_sidetrack_const() {
  for (var i = 0; i < 3; ++i) {
    if (i == 1) {
      %OptimizeOsr();
    }
    var a = 0;
    function inner() {
      return a;
      const {b: {a}, c} = {b: {a: 1}, c: 2};
    }
  }
}
%PrepareFunctionForOptimization(TestRegress650969_sidetrack_const);
TestRegress650969_sidetrack_const();
