//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

for (var i = 0; i < 4 * 60; i++) {
    var d = new Date(2012, 2, 11, 0, i, 0);
    WScript.Echo(d.toString());
}
for (var i = 0; i < 4 * 60; i++) {
    var d = new Date(2012, 10, 4, 0, i, 0);
    WScript.Echo(d.toString());
}

// BLUE: 538457
var bug538457 = new Date(1383672529000000);
WScript.Echo(bug538457.toString());
