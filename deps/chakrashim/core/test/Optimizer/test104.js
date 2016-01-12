//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    var obj1 = { prop1: 0.1 };
    obj1.prop1;
    for(var i = 0; i < 1; ++i) {
        test0a();
        var x = obj1.prop1;
        x += 1;
        WScript.Echo(x);
    }

    function test0a() {
        try {
            test0b();
        } catch(ex) {
        }

        function test0b() {
            for(var i = 0; i < 1; ++i) {
                0.1 - _nonexistent;
                break;
            }
        }
    }
};
test0();
test0();
