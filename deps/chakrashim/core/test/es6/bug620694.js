//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// OS: Bug 620694: Assertion when evaluating 'new Map();' in F12
//
//     Object.toString() incorrectly returns Var from temporary allocator.
//
// Run with: -es6all  (To make it more likely to repro, add -recyclerstress)
//

/// <reference path="../UnitTestFramework/UnitTestFramework.js" />
WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");


// Bug: "x" is from temp allocator. Supposed to contain string "[object Map]".
var x = (new Map()).toString();

// Try to overwrite memory of "x" with other similar Vars also from temp allocator, "[object Set]".
for (var i = 0; i < 10; i++) {
    var tmp = new Set();
    tmp = tmp.toString();
}

assert.areEqual("[object Map]", x);
WScript.Echo("pass");
