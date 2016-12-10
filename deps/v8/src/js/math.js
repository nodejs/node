// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {
"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

// The first two slots are reserved to persist PRNG state.
define kRandomNumberStart = 2;

var GlobalMath = global.Math;
var NaN = %GetRootNaN();
var nextRandomIndex = 0;
var randomNumbers = UNDEFINED;
var toStringTagSymbol = utils.ImportNow("to_string_tag_symbol");

//-------------------------------------------------------------------
// ECMA 262 - 15.8.2.14
function MathRandom() {
  // While creating a startup snapshot, %GenerateRandomNumbers returns a
  // normal array containing a single random number, and has to be called for
  // every new random number.
  // Otherwise, it returns a pre-populated typed array of random numbers. The
  // first two elements are reserved for the PRNG state.
  if (nextRandomIndex <= kRandomNumberStart) {
    randomNumbers = %GenerateRandomNumbers(randomNumbers);
    if (%_IsTypedArray(randomNumbers)) {
      nextRandomIndex = %_TypedArrayGetLength(randomNumbers);
    } else {
      nextRandomIndex = randomNumbers.length;
    }
  }
  return randomNumbers[--nextRandomIndex];
}

// -------------------------------------------------------------------

%AddNamedProperty(GlobalMath, toStringTagSymbol, "Math", READ_ONLY | DONT_ENUM);

// Set up non-enumerable functions of the Math object and
// set their names.
utils.InstallFunctions(GlobalMath, DONT_ENUM, [
  "random", MathRandom,
]);

%SetForceInlineFlag(MathRandom);

// -------------------------------------------------------------------
// Exports

utils.Export(function(to) {
  to.MathRandom = MathRandom;
});

})
