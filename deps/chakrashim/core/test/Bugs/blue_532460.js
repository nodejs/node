//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var x = function () {
    return this
};

function test4() {
    Object.prototype['create'] = function () {
        return this
    };
    var Proxy = function () {
        return this
    };
    (function () {
        lofxue(y = Proxy.create((function handlerFactory(x) {
            return {
                enumerate : function () {
                    return [];
                },
            };
    })(/x/g), "uC5EA"));
        function lofxue() {
            for (let d in[function () {}, [(void 0)], [(void 0)], NaN]) {
                CollectGarbage();
            }
        }; ;
    })();
    var w;
};
test4();
WScript.Echo("Passed");
