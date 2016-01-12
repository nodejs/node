//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    var obj0 = {};
    var v9449 = {
            create: function () {
                return function bar() {
                    arguments[2];
                    this.method0.apply(this, arguments);
                };
            }
        };
    var v9451 = obj0;
    v9451.v9452 = v9449.create();
    v9451.v9452.prototype = {
        method0: function () {
            this;
        }
    };
    v9451.v9454 = v9449.create();
    v9451.v9454.prototype = {
        method0: function () {
            this.v9459 = new v9451.v9452();
        }
    };
    var v9471 = new v9451.v9454();
    var v9472 = new v9451.v9454();
}
test0();
WScript.Echo("Passed");
