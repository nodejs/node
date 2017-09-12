// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Logical Expressions
AddTest('Or-Value', '||', false);
AddTest('Or-Test', '||', true);
AddTest('And-Value', '&&', false);
AddTest('And-Test', '&&', true);
AddTest('Comma-Value', ',', false);
AddTest('Comma-Test', ',', true);
AddTest('Comma-Test', ',', true);

// Compare Expressions
AddTest('Equals-Value', '==', false);
AddTest('Equals-Test', '==', true);
AddTest('StrictEquals-Value', '===', false);
AddTest('StrictEquals-Test', '===', true);
AddTest('GreaterThan-Value', '>', false);
AddTest('GreaterThan-Test', '>', true);

// Binary Expressions
AddTest('Add', '+');
AddTest('Sub', '-');
AddTest('BitwiseOr', '|');
AddTestCustomPrologue('StringConcat', '+', '"string" +');

function TestExpressionDepth(depth, expression, prologue, epilogue) {
  var func = '(function f(a) {\n' + prologue;
  for (var i = 0; i < depth; i++) {
    func += 'a ' + expression;
  }
  func += 'a' + epilogue + '})();'
  eval(func);
}

function RunTest(name, expression, prologue, epilogue) {
  var depth;
  try {
    for (depth = 0; depth < 20000; depth += 100) {
      TestExpressionDepth(depth, expression, prologue, epilogue);
    }
  } catch (e) {
    if (!e instanceof RangeError) {
      print(name +  '-ExpressionDepth(Score): ERROR');
      return;
    }
  }
  print(name + '-ExpressionDepth(Score): ' + depth);
}

function AddTest(name, expression, in_test) {
  prologue = '';
  epilogue = '';
  if (in_test) {
    prologue = 'if (';
    epilogue = ') { return 1; }';
  }
  RunTest(name, expression, prologue, epilogue);
}

function AddTestCustomPrologue(name, expression, prologue) {
  RunTest(name, expression, prologue, '');
}
