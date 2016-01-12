//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// A specific case that was regressed
var toSubtract = 1099511627776;
writeLine("toSubtract:\n    " + toSubtract);
var toMultiply = 0.840812748240128;
writeLine("toMultiply:\n    " + toMultiply);
var powResult = Math.pow(2, 55);
writeLine("powResult:\n    " + powResult);
var subtracted = powResult - toSubtract;
writeLine("subtracted:\n    " + subtracted);
var multiplied = toMultiply * subtracted;
writeLine("multiplied:\n    " + multiplied);
var stringed = "" + multiplied;
writeLine("stringed:\n    " + stringed);
var evaled = eval(stringed);
writeLine("evaled:\n    " + evaled);

(function testInlineParameterSideEffects() {
  function foo(){
    var a = 12345;
    var x = Math.pow(a, 1 >> (a = 0));
    return x;
  }
  var x = foo();
  WScript.Echo("testInlineParameterSideEffects:\n    " + x);
})();

(function testInlineWin8_748804() {
  var result;
  function decimalToHexString(n) {
    for (var i = 1; i >= 1; --i) {
      if (n >= Math.pow(16, i)) {
        var t = Math.floor(n / Math.pow(16, i));
        result = t;
        n = t * Math.pow(16, i);
      }
    }
  }
  decimalToHexString(0xDF);
  WScript.Echo("testInlineWin8_748804:\n    " + result);
})();

safeCall(function testInlineMathBuiltinCalledAsConstructor() {
    var x = new Math.sin(0);
});

// Helpers

function writeLine(str)
{
    WScript.Echo("" + str);
}

function safeCall(f) {
    var args = [];
    for(var a = 1; a < arguments.length; ++a)
        args.push(arguments[a]);
    try {
        return f.apply(this, args);
    } catch(ex) {
        writeLine(ex.name);
    }
}
