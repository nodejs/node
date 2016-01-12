//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Simulate nested calls to setTimeout by setting and calling a callback variable

var callback;
callback = (function () {
    WScript.Echo('callback 1');
    var ran = false;
    function startTest() {
        WScript.Echo('startTest: ran == ' + ran);
        if (!ran) {
            ran = true;
            try {
                callback = (function () {
                    WScript.Echo('callback 2');
                    // Needs the timeout nested call to crash
                    startTest();
                    callback = null;
                });
            } catch (e) {
                callback = (function () {
                    WScript.Echo('callback 2');
                    // Needs the closure reference to e, to crash
                    var x = e;
                    callback = null;
                });
            }
        }
    }
    startTest();
});

while (callback)
    callback();
