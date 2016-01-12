//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Test binary operations with potential side-effects on already-evaluated opnds.

function foo() {
    var x = 0;
    var z = x & (x = 1)
    WScript.Echo(z)
    x = 0;
    x &= (x |= 1);
    WScript.Echo(x);
}
foo();

(function () {

    var f = 5;

    x = (f * (f++));

    WScript.Echo("x = " + x);

})();

var o = new Object();
function func(b) {
    b.blah = b.blah2 = b = null;
}

func(o);


