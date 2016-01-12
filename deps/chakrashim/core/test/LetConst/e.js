//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var x = 10;
function f() {

    const x = 5;
    {
        const x = "abacaba";
        WScript.Echo(x);
        {
            const x = 111111;
            WScript.Echo(x);
        }
        WScript.Echo(x);
        {
           const x = 222222;
            WScript.Echo(x);
        }
        WScript.Echo(x);
    }
    WScript.Echo(x);
};
f();
WScript.Echo(x);
