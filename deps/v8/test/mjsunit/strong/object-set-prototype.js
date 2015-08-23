// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode --allow-natives-syntax

// TODO(conradw): Track implementation of strong bit for other objects, add
// tests.

function getSloppyObjects() {
  return [(function(){}), ({})];
}

function getStrictObjects() {
  "use strict";
  return [(function(){}), ({})];
}

function getStrongObjects() {
  "use strong";
  return [(function(){}), ({})];
}

function declareObjectLiteralWithProtoSloppy() {
  return {__proto__: {}};
}

function declareObjectLiteralWithProtoStrong() {
  "use strong";
  return {__proto__: {}};
}

function testStrongObjectSetProto() {
  "use strict";
  let sloppyObjects = getSloppyObjects();
  let strictObjects = getStrictObjects();
  let strongObjects = getStrongObjects();
  let weakObjects = sloppyObjects.concat(strictObjects);

  for (let o of weakObjects) {
    let setProtoBuiltin = function(o){Object.setPrototypeOf(o, {})};
    let setProtoProperty = function(o){o.__proto__ = {}};
    for (let setProtoFunc of [setProtoBuiltin, setProtoProperty]) {
      assertDoesNotThrow(function(){setProtoFunc(o)});
      assertDoesNotThrow(function(){setProtoFunc(o)});
      assertDoesNotThrow(function(){setProtoFunc(o)});
      %OptimizeFunctionOnNextCall(setProtoFunc);
      assertDoesNotThrow(function(){setProtoFunc(o)});
      %DeoptimizeFunction(setProtoFunc);
      assertDoesNotThrow(function(){setProtoFunc(o)});
    }
  }
  for (let o of strongObjects) {
    let setProtoBuiltin = function(o){Object.setPrototypeOf(o, {})};
    let setProtoProperty = function(o){o.__proto__ = {}};
    for (let setProtoFunc of [setProtoBuiltin, setProtoProperty]) {
      assertThrows(function(){setProtoFunc(o)}, TypeError);
      assertThrows(function(){setProtoFunc(o)}, TypeError);
      assertThrows(function(){setProtoFunc(o)}, TypeError);
      %OptimizeFunctionOnNextCall(setProtoFunc);
      assertThrows(function(){setProtoFunc(o)}, TypeError);
      %DeoptimizeFunction(setProtoFunc);
      assertThrows(function(){setProtoFunc(o)}, TypeError);
    }
  }

  assertDoesNotThrow(declareObjectLiteralWithProtoSloppy);
  assertDoesNotThrow(declareObjectLiteralWithProtoSloppy);
  assertDoesNotThrow(declareObjectLiteralWithProtoSloppy);
  %OptimizeFunctionOnNextCall(declareObjectLiteralWithProtoSloppy);
  assertDoesNotThrow(declareObjectLiteralWithProtoSloppy);
  %DeoptimizeFunction(declareObjectLiteralWithProtoSloppy);
  assertDoesNotThrow(declareObjectLiteralWithProtoSloppy);

  assertDoesNotThrow(declareObjectLiteralWithProtoStrong);
  assertDoesNotThrow(declareObjectLiteralWithProtoStrong);
  assertDoesNotThrow(declareObjectLiteralWithProtoStrong);
  %OptimizeFunctionOnNextCall(declareObjectLiteralWithProtoStrong);
  assertDoesNotThrow(declareObjectLiteralWithProtoStrong);
  %DeoptimizeFunction(declareObjectLiteralWithProtoStrong);
  assertDoesNotThrow(declareObjectLiteralWithProtoStrong);
}
testStrongObjectSetProto();
