//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function assertPropertyExists(o, p, v) {
    if (!o.hasOwnProperty(p)) {
        throw new Error("Object does not have expected property '" + p + "'");
    }
    if (o[p] !== v) {
        throw new Error("Object has property '" + p + "' but its value does not match the expected value");
    }
}

function assertPropertyDoesNotExist(o, p) {
    if (o.hasOwnProperty(p)) {
        throw new Error("Object has unexpected property '" + p + "'");
    }
}

WScript.LoadScriptFile("vso_os_1091425_1.js");
WScript.LoadScriptFile("vso_os_1091425_2.js");
try {
    eval('function nonConfigurableFoo() { /* try to override non-configurable global accessor property with a function definition */ }');
} catch (e) {
    if (e.message === "Cannot redefine non-configurable property 'nonConfigurableFoo'") {
        print("Pass");
    }
    else {
        print("Fail");
    }
}
