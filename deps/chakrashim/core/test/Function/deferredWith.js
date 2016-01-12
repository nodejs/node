//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0(x) {
    with (x)
    {
        z = 
            function handlerFactory() {
            return { test: 
                 function () { return "4"; } };
            },

        z2 =
            function handlerFactory2() {
                return {
                    test:
                         function a() { return "5"; }
                };
            }
    };;

    var handlerFactory = handlerFactory || undefined;
    var handlerFactory2 = handlerFactory2 || undefined;

    return { x: x, handlerFactory: handlerFactory, handlerFactory2: handlerFactory2 };
};

var p={o:1, z:2, z2:3};
WScript.Echo("p = " + JSON.stringify(p));
var testOut=test0(p);
var k = testOut.x;
WScript.Echo("k = " + JSON.stringify(k));
WScript.Echo("k.z = " + k.z);
WScript.Echo("k.z() = " + k.z());
WScript.Echo("k.z().test() = " + JSON.stringify(k.z().test()));
WScript.Echo("k.z().test()+1 = " + JSON.stringify(k.z().test()+1));

WScript.Echo();
WScript.Echo("sibbling with block");
WScript.Echo("k.z2 = " + k.z2);
WScript.Echo("k.z2() = " + k.z2());
WScript.Echo("k.z2().test() = " + JSON.stringify(k.z2().test()));
WScript.Echo("k.z2().test()+1 = " + JSON.stringify(k.z2().test() + 1));

WScript.Echo();
WScript.Echo("compat mode specifics as !== undefined (if present) - version:2 specifics");
if (testOut.handlerFactory !== undefined) {
    WScript.Echo("testOut.handlerFactory().test() = " + JSON.stringify(testOut.handlerFactory().test()) + " (as json)");
    WScript.Echo("testOut.handlerFactory().test()+1 = " + testOut.handlerFactory().test() + 1);
}
if (testOut.handlerFactory2 !== undefined) {
    WScript.Echo("testOut.handlerFactory2().test() = " + JSON.stringify(testOut.handlerFactory2().test()) + " (as json)");
    WScript.Echo("testOut.handlerFactory2().test()+1 = " + testOut.handlerFactory2().test() + 1);
}