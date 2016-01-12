//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    var loopInvariant = 2;
    var func2 = function () {
        function func5() {
        }
        loopInvariant;
        {
            function __f() {
                WScript.Echo('pass');
            }
            function __g() {
                __f();
            }
            __g();
        }
    };
    func2();
}
test0();
