//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//
// Test etw rundown.
//

function bar() {
    var s = "";
    for (var i = 0; i < 40; i++) {
        s += "bar";
    }
    (function (a) { })(s);
}

var MAX_TIME = 2000; //max ms to run the following loop (and bound etw rundown time)
var beginTime = new Date();

// Keep generating new functions.
for (var i = 0; i < 1000; i++) {
    var foo = "foo" + i; // different function names
    eval("function " + foo + "(){ bar();} " + foo + "();");

    var now = new Date();
    if (now - beginTime >= MAX_TIME) {
        break;
    }
}

WScript.Echo("pass");