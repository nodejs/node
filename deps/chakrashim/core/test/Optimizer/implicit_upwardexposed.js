//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    function func0() {
        return -(this.prop0 = 0.1);
    }
    return new func0().prop0;
}
WScript.Echo(test0());
Object.defineProperty(
    Object.prototype, 'prop0', {
        get: function () {
            return "get";
        },
        set: function (a) {
            WScript.Echo("set:" + a);
        }
    }
);
WScript.Echo(test0());
