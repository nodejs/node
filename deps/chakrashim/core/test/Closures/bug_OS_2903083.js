//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var test = function () {
    function x() { }
    {
        function x() { }
    }
    function y() {
    }
    return eval('new y()');
};
var a, b;
(a = test()) + (b = test());
if (Object.getPrototypeOf(a) === Object.getPrototypeOf(b)) {
    WScript.Echo("failed");
} else {
    WScript.Echo("passed");
}
