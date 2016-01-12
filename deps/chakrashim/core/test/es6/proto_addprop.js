//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

/// <reference path="../UnitTestFramework/UnitTestFramework.js" />
if (this.WScript && this.WScript.LoadScriptFile) {
    WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");
}


var p = { pp: 123 };

function F() { this.dummy = 12; /*reserve slots, make jit code simpler to read*/ }
F.prototype = p;

function make_object() {
    /// Create new objects of the same Type, and with __proto__ "p"
    return new F();
}

function foo(o) {
    o.x = 1;
    o.y = 2;
}

// Need to run this twice. Test with maxinterpretcount 1 and 2
foo(make_object());
foo(make_object());

var o3 = make_object();

assert.isTrue(Object.getPrototypeOf(o3) === p);
p.__proto__ = { get x() { return "x"; } };

foo(o3);

assert.areEqual("x", o3.x, "Shouldn't add field x");

WScript.Echo("pass");
