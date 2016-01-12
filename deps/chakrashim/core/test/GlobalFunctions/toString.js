//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

write(toString(["a"]));

function toString(o, quoteStrings) {
    if (quoteStrings === true) {
        return "recursive call"
    }

    eval("");

    for (var i = 0; i < o.length; ++i) {
        return toString(o[0], true);
    }
}


var o = {};
Object.prototype.x = 20;
var x = 10;

with (o) {
    write(x);
}