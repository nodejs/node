//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

for (var i = 0; i < 3; i++) {
    Object.prototype['fireEvent'] = function () {
        return this;
    };
    var window = function () {
    };
    (function () {
        if (window.fireEvent()) {
            +window;
        }
    })();
}
WScript.Echo('pass');
