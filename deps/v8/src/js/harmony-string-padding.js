// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalString = global.String;
var MakeTypeError;

utils.Import(function(from) {
  MakeTypeError = from.MakeTypeError;
});

// -------------------------------------------------------------------
// http://tc39.github.io/proposal-string-pad-start-end/

function StringPad(thisString, maxLength, fillString) {
  maxLength = TO_LENGTH(maxLength);
  var stringLength = thisString.length;

  if (maxLength <= stringLength) return "";

  if (IS_UNDEFINED(fillString)) {
    fillString = " ";
  } else {
    fillString = TO_STRING(fillString);
    if (fillString === "") {
      // If filler is the empty String, return S.
      return "";
    }
  }

  var fillLength = maxLength - stringLength;
  var repetitions = (fillLength / fillString.length) | 0;
  var remainingChars = (fillLength - fillString.length * repetitions) | 0;

  var filler = "";
  while (true) {
    if (repetitions & 1) filler += fillString;
    repetitions >>= 1;
    if (repetitions === 0) break;
    fillString += fillString;
  }

  if (remainingChars) {
    filler += %_SubString(fillString, 0, remainingChars);
  }

  return filler;
}

function StringPadStart(maxLength, fillString) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.padStart")
  var thisString = TO_STRING(this);

  return StringPad(thisString, maxLength, fillString) + thisString;
}
%FunctionSetLength(StringPadStart, 1);

function StringPadEnd(maxLength, fillString) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.padEnd")
  var thisString = TO_STRING(this);

  return thisString + StringPad(thisString, maxLength, fillString);
}
%FunctionSetLength(StringPadEnd, 1);

utils.InstallFunctions(GlobalString.prototype, DONT_ENUM, [
  "padStart", StringPadStart,
  "padEnd", StringPadEnd
]);

});
