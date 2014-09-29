// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// TODO(jkummerow): There are many ways to improve these tests, e.g.:
// - more variance in randomized inputs
// - better time complexity management
// - better code readability and documentation of intentions.

var RUN_WITH_ALL_ARGUMENT_ENTRIES = false;
var kOnManyArgumentsRemove = 5;

function makeArguments() {
  var result = [ ];
  result.push(17);
  result.push(-31);
  result.push(new Array(100));
  var a = %NormalizeElements([]);
  a.length = 100003;
  result.push(a);
  result.push(Number.MIN_VALUE);
  result.push("whoops");
  result.push("x");
  result.push({"x": 1, "y": 2});
  var slowCaseObj = {"a": 3, "b": 4, "c": 5};
  delete slowCaseObj.c;
  result.push(slowCaseObj);
  result.push(function () { return 8; });
  return result;
}

var kArgObjects = makeArguments().length;

function makeFunction(name, argc) {
  var args = [];
  for (var i = 0; i < argc; i++)
    args.push("x" + i);
  var argsStr = args.join(", ");
  return new Function(argsStr,
                      "return %" + name + "(" + argsStr + ");");
}

function testArgumentCount(name, argc) {
  for (var i = 0; i < 10; i++) {
    var func = null;
    try {
      func = makeFunction(name, i);
    } catch (e) {
      if (e != "SyntaxError: Illegal access") throw e;
    }
    if (func === null && i == argc) {
      throw "unexpected exception";
    }
    var args = [ ];
    for (var j = 0; j < i; j++)
      args.push(0);
    try {
      func.apply(void 0, args);
    } catch (e) {
      // we don't care what happens as long as we don't crash
    }
  }
}

function testArgumentTypes(name, argc) {
  var type = 0;
  var hasMore = true;
  var func = makeFunction(name, argc);
  while (hasMore) {
    var argPool = makeArguments();
    // When we have 5 or more arguments we lower the amount of tests cases
    // by randomly removing kOnManyArgumentsRemove entries
    var numArguments = RUN_WITH_ALL_ARGUMENT_ENTRIES ?
      kArgObjects : kArgObjects - kOnManyArgumentsRemove;
    if (kArgObjects >= 5 && !RUN_WITH_ALL_ARGUMENT_ENTRIES) {
      for (var i = 0; i < kOnManyArgumentsRemove; i++) {
        var rand = Math.floor(Math.random() * (kArgObjects - i));
        argPool.splice(rand, 1);
      }
    }
    var current = type;
    hasMore = false;
    var argList = [ ];
    for (var i = 0; i < argc; i++) {
      var index = current % numArguments;
      current = (current / numArguments) << 0;
      if (index != (numArguments - 1))
        hasMore = true;
      argList.push(argPool[index]);
    }
    try {
      func.apply(void 0, argList);
    } catch (e) {
      // we don't care what happens as long as we don't crash
    }
    type++;
  }
}

testArgumentCount(NAME, ARGC);
testArgumentTypes(NAME, ARGC);
