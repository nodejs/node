//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: 'Quantifier characters shouldn\'t be allowed on their own',
        body: function () {
            assert.throws(function () { eval('/*/'); }, SyntaxError, '/*/');
            assert.throws(function () { eval('/+/'); }, SyntaxError, '/+/');
            assert.throws(function () { eval('/?/'); }, SyntaxError, '/?/');
        }
    }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != 'summary' });
