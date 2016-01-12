//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

try {
    eval(
        "function f() {" +
        "    function foo3() {" +
            "    function eval(a) {" +
                "    \"use strict\";" +
            "    }" +
        "    };" +
        "    foo3();" +
        "}");
    f();
}
catch(e) {
    WScript.Echo(e.message);
}

try {
    eval(
        "function f() {" +
        "    function foo3() {" +
            "    function a(eval) {" +
                "    \"use strict\";" +
            "    }" +
        "    };" +
        "    foo3();" +
        "}");
    f();
}
catch(e) {
    WScript.Echo(e.message);
}
