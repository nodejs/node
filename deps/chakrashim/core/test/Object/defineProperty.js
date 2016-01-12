//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function TestDefineProperty(desc, testName) {
    WScript.Echo(testName);
    CatchAndWriteExceptions(function () {
        Object.defineProperty(new Object(), "foo", desc);
        WScript.Echo("Success");
    });
}

function CatchAndWriteExceptions(func) {
    try {
        func();
    }
    catch (e) {
        WScript.Echo(e.name + ": " + e.number);
    }
}

var bools = [true, false];
var boolsExtended = [true, false];
boolsExtended[2] = undefined;  // Work around WOOB 1099317 in compat mode

var desc;

for (var includeValue in bools) {
    for (var includeWritable in boolsExtended) {
        for (var includeGetter in bools) {
            for (var includeSetter in bools) {
                var s = "";
                var b;
                desc = {};

                b = bools[includeValue];
                if (b) { desc.value = "fooValue"; s += "value; "; }

                b = boolsExtended[includeWritable];
                if (b !== undefined) { desc.writable = b; s += "writable=" + b + "; "; }

                b = bools[includeGetter];
                if (b) { desc.get = function () { return "aValue"; }; s += "getter; "; }

                b = bools[includeSetter];
                if (b) { desc.set = function (v) { }; s += "setter; "; }

                TestDefineProperty(desc, s);
            }
        }
    }
}
