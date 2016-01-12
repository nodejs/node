//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// These tests are to validate the functionality of UnitTestFramework.js.
// Other unit tests rely on UTF, so it's super-imporant for UTF to be bug-free.

// Note: run this using jshost.exe, jc.exe wouldn't find WScript.LoadScriptFile.
if (typeof(WScript) != "undefined") {
  WScript.LoadScriptFile("UnitTestFramework.js");
}

helpers.writeln("*** Validating assert.throws...");
assert.throws(function() { throw "this throws."; });
helpers.writeln("assert.throws(function) -- function that throws: PASS");
try {
  assert.throws(function(){});
  helpers.writeln("assert.throws(function) -- function that doen't throw: FAIL");
} catch (ex) {
  helpers.writeln("assert.throws(function) -- function that doesn't throw: PASS");
}
assert.throws(function() { assert.throws(function() {}) });
// Simple form of assert.throws is what all further validations are based on.
// Now after it's validated to be correct, use it for everything else.
assert.throws(function() { eval("{"); }, SyntaxError);
assert.throws(function() { assert.throws(function() {}, SyntaxError) });
helpers.writeln("assert.throws(function, Exception): PASS");

helpers.writeln("*** Validating assert.fail...");
assert.throws(function() { assert.fail(); });
helpers.writeln("assert.fail: PASS");

helpers.writeln("*** Validating assert.areEqual...");
assert.areEqual(0, 0);
assert.areEqual(5, 5);
assert.areEqual(true, true);
assert.areEqual(false, false);
assert.areEqual("test", "test");
var obj1 = {x:0}, obj2 = {x:1};
assert.areEqual(obj1, obj1);
assert.throws(function() { assert.areEqual(0, 1) });
assert.throws(function() { assert.areEqual(100, 200) });
assert.throws(function() { assert.areEqual(0, "0") });
assert.throws(function() { assert.areEqual(true, false) });
assert.throws(function() { assert.areEqual("xy", "x") });
assert.throws(function() { assert.areEqual(obj1, obj2) });
helpers.writeln("assert.areEqual: PASS");

helpers.writeln("*** Validating assert.areNotEqual...");
assert.throws(function() { assert.areNotEqual(0, 0); }); 
assert.throws(function() { assert.areNotEqual(5, 5); }); 
assert.throws(function() { assert.areNotEqual(true, true); } );
assert.throws(function() { assert.areNotEqual(false, false); } );
assert.throws(function() { assert.areNotEqual("test", "test"); });
var obj1 = {x:0}, obj2 = {y:0};
assert.throws(function() { assert.areNotEqual(obj1, obj1); });
assert.areNotEqual(0, 1);
assert.areNotEqual(100, 200);
assert.areNotEqual(0, "0");
assert.areNotEqual(true, false);
assert.areNotEqual("xy", "x");
assert.areNotEqual(obj1, obj2);
helpers.writeln("assert.areNotEqual: PASS");

helpers.writeln("*** Validating assert.isTrue/isFalse...");
assert.isTrue(true);
assert.throws(function() { assert.isTrue(false); }); 
assert.isFalse(false);
assert.throws(function() { assert.isFalse(true); }); 
helpers.writeln("assert.isTrue/isFalse: PASS");

helpers.writeln("*** Validating isCompatVersion9...");
assert.isTrue(helpers.isCompatVersion9 === !helpers.isVersion10OrLater);
helpers.writeln("Validating isCompatVersion9: PASS");
