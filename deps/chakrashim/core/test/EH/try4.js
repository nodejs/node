//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test(i) {
    try {
        var g = function (x) {
                    try {
                        eval("throw x;");
                    }
                    catch(b) {
                        WScript.Echo("g: caught " + b);
                    }
                };
        throw g;
    }
    catch (a) {
        return a;
    }
};

try {
    var g = function (x) {
                try {
                    throw x;
                }
                catch(b) {
                    WScript.Echo("g: caught " + b);
                    return x;
                }
            };
    throw g;
}
catch (a) {
    eval("WScript.Echo(a(6))");
}

test(1)(2);
