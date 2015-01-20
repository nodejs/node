// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file relies on the fact that the following declarations have been made
// in runtime.js:
// var $Function = global.Function;
// var $Array = global.Array;

"use strict";

function FunctionToMethod(homeObject) {
  if (!IS_SPEC_FUNCTION(this)) {
    throw MakeTypeError('toMethod_non_function',
                        [%ToString(this), typeof this]);

  }

  if (!IS_SPEC_OBJECT(homeObject)) {
    throw MakeTypeError('toMethod_non_object',
                        [%ToString(homeObject)]);
  }

  return %ToMethod(this, homeObject);
}

function SetupHarmonyClasses() {
  %CheckIsBootstrapping();

  InstallFunctions($Function.prototype, DONT_ENUM, $Array(
      "toMethod", FunctionToMethod
  ));
}

SetupHarmonyClasses();
