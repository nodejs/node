//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//                   000000000111111111122222222223333333333444444444455555555556666666666777777777788888888889
//                   123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890
var expectedError = "Error: aİc\n\tat aTurkish (bug_258259.js:12:5)\n\tat Global code (bug_258259.js:34:9)";

//-- Turkish 'i' character in error message and file name
function aTurkish() {
    throw Error("aİc");
}

function filterFullFilePathFromCallstack(cs) {
    var filteredStack = cs;
    var fileName = "bug_258259.js:";
    var startDelim = " (";

    // remove full path from the file name in the call stack (x2)
    var lastInd = filteredStack.lastIndexOf(fileName);
    var firstInd = filteredStack.lastIndexOf(startDelim, lastInd);
    filteredStack = filteredStack.substring(0, firstInd + startDelim.length) + filteredStack.substring(lastInd);

    lastInd = filteredStack.lastIndexOf(fileName);
    lastInd = filteredStack.lastIndexOf(fileName, lastInd - 1);
    firstInd = filteredStack.lastIndexOf(startDelim, lastInd);
    filteredStack = filteredStack.substring(0, firstInd + startDelim.length) + filteredStack.substring(lastInd);

    return filteredStack;
}

try {
        aTurkish();
} catch (ex) {
    var filteredStack = filterFullFilePathFromCallstack([ex.stack].toString());

    if (filteredStack == expectedError) {
        WScript.Echo("PASSED");
    } else {
        WScript.Echo("FAILED");
        WScript.Echo("\nActual (raw):\n" + [ex.stack]);
        WScript.Echo("\nActual (filtered):\n" + filteredStack);
        WScript.Echo("\n\nExpected:\n" + expectedError);
    }
}
