//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// -mic:2 -off:simplejit -force:rejit

function leaf() {
}
var obj0 = {};
obj0 = leaf();

function test0() {
    var count = 0;
    var litObj0 = { prop1: 0 };
    function func1() {
        litObj0.prop0 = 1;
    };
    Object.defineProperty(litObj0, 'prop0', {
        set: function () {
            count++;
            WScript.Echo(count);
        }
    });
    
    func1();
    func1();
}
test0();
test0();