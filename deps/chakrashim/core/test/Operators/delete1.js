//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

var r;

function doEval(str) {    
    try {
        eval("r = " + str + ";  write(r);");
        write("No exception: " + str);
    } catch (e) {
        write("Exception " + str + ": " + e.message);
    }    
}

var consts = [
    "true", "false", "10", "10.12", '"hello"', "null", 
    "undefined", "new Object()", "new Number(10)", "new Boolean(true)", "new Date()", "new String('hello')",
    "new Function('return 10')", "new Array(10)"
];

// Delete constants
for (var i=0; i<consts.length; i++) {
    doEval("delete " + consts[i]);    
}