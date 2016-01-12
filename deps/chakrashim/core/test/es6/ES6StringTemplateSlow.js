//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "Octal escape sequences are not allowed in string template literals - exhaustive test",
        body: function() {
            function verifyOctalThrows(octalNumber) {
                if (octalNumber < 10) {
                    assert.throws(function () { eval('print(`\\00' + octalNumber + '`)'); }, SyntaxError, "Scanning an octal escape sequence " + "`\\00" + octalNumber + "` throws SyntaxError.", "Octal numeric literals and escape characters not allowed in strict mode");
                }
                if (octalNumber < 100) {
                    assert.throws(function () { eval('print(`\\0' + octalNumber + '`)'); }, SyntaxError, "Scanning an octal escape sequence " + "`\\0" + octalNumber + "` throws SyntaxError.", "Octal numeric literals and escape characters not allowed in strict mode");
                }
                assert.throws(function () { eval('print(`\\' + octalNumber + '`)'); }, SyntaxError, "Scanning an octal escape sequence " + "`\\" + octalNumber + "` throws SyntaxError.", "Octal numeric literals and escape characters not allowed in strict mode");
            }
            for (var i = 1; i <= 255; i++) {
                verifyOctalThrows(i.toString(8));
            }
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
