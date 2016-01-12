//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo1() {
    var ary = Array();
    var foo1Var = 1;
    function foo2() {
        var foo2Var = 0;
        var err = Error();
        try {
            throw err;
        } catch (ex) { }
        while (true) {
            function foo4() {
                foo5();
            }
            foo1Var = ary;
            function foo5() {
                ary[foo2Var] = foo1Var;
            }
            foo5(ary);
            break;
        }
    }
    foo2();
    WScript.Echo(typeof ary[0]);
};
foo1();
