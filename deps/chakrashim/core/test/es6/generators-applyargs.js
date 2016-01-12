//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function f() {
    var s = "";
    for (var i in arguments) {
        s += arguments[i];
    }
    WScript.Echo(s);
}

function* gf() {
    f.apply(this, arguments);
}

var g = gf('p', 'a', 's', 's', 'e', 'd');
g.next();
