//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var x = { a: "x.a", f: function() { return this.a; } };
var y = { a: "y.a", f: function() { return this.a; } };
var a = "glo.a";
function f() {
    return this.a;
}

with (x) {
    with (y) {
        eval("WScript.Echo(f())");
        eval("WScript.Echo(x.f(), f())");
    }
    eval("WScript.Echo(x.f(), f())");
}
eval("WScript.Echo(y.f(), x.f(), f())");

with (Math) {
    with (9) {
        with (8) {
            with (7) {
                with (6) {
                    WScript.Echo(eval("abs(valueOf())"));
                }
            }
        }
    }
}
