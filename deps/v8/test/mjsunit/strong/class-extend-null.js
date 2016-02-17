// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode --allow-natives-syntax

(function() {
"use strict";

let foo = null;

function nullLiteral() {
  class Class1 extends null {
    constructor() {
      super();
    }
  }
}

function nullVariable() {
  class Class2 extends foo {
    constructor() {
      super();
    }
  }
}

function nullLiteralClassExpr() {
  (class extends null {});
}

function nullVariableClassExpr() {
  (class extends foo {});
}

assertDoesNotThrow(nullLiteral);
%OptimizeFunctionOnNextCall(nullLiteral);
assertDoesNotThrow(nullLiteral);

assertDoesNotThrow(nullVariable);
%OptimizeFunctionOnNextCall(nullVariable);
assertDoesNotThrow(nullVariable);

assertDoesNotThrow(nullLiteralClassExpr);
%OptimizeFunctionOnNextCall(nullLiteralClassExpr);
assertDoesNotThrow(nullLiteralClassExpr);

assertDoesNotThrow(nullVariableClassExpr);
%OptimizeFunctionOnNextCall(nullVariableClassExpr);
assertDoesNotThrow(nullVariableClassExpr);
})();

(function() {
"use strong";

let foo = null;

function nullLiteral() {
  class Class1 extends null {
    constructor() {
      super();
    }
  }
}

function nullVariable() {
  class Class2 extends foo {
    constructor() {
      super();
    }
  }
}

function nullLiteralClassExpr() {
  (class extends null {});
}

function nullVariableClassExpr() {
  (class extends foo {});
}

assertThrows(nullLiteral, TypeError);
%OptimizeFunctionOnNextCall(nullLiteral);
assertThrows(nullLiteral, TypeError);

assertThrows(nullVariable, TypeError);
%OptimizeFunctionOnNextCall(nullVariable);
assertThrows(nullVariable, TypeError);

assertThrows(nullLiteralClassExpr, TypeError);
%OptimizeFunctionOnNextCall(nullLiteralClassExpr);
assertThrows(nullLiteralClassExpr, TypeError);

assertThrows(nullVariableClassExpr, TypeError);
%OptimizeFunctionOnNextCall(nullVariableClassExpr);
assertThrows(nullVariableClassExpr, TypeError);
})();
