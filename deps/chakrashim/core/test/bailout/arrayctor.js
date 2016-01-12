//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function f(x, y) {
    return new Array(x, y);
}

f(1, 2);
f(2, 3);
Array = function (x, y) { WScript.Echo('arg: ' + x + ', ' + y); }
f(3, 4);
f(5, 6);
