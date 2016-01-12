//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

if (this.WScript && this.WScript.LoadScriptFile) { // works for browser
    WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");
}

var tests = [
{
    name: "regress Win8: 690708",
    body: function () {

        function stringify(o, space) {
            var str = JSON.stringify(o, null, space);
            var str2 = JSON.stringify(o, null, new Number(space)); // Test Number Object

            helpers.writeln("--space: " + space);
            helpers.writeln(str);
            assert.areEqual(str, str2);
        }
        
        var o = { ab: 123 };
        var spaces = [
            Number.MIN_VALUE,
            -4294967296,
            -2147483649,
            -2147483648, //int32 min
            -1073741825,
            -1073741824, //int31 min
            -28, -7, -1, 0, 1, 6, 15,
            1073741823, //int31 max
            1073741824,
            2147483647, //int32 max
            2147483647.1,
            2147483648,
            2147483648.2,
            4294967295, //uint32 max
            4294967296,
            Number.MAX_VALUE
        ];
        spaces.forEach(function (space) {
            stringify(o, space);
        });
    }
}
];

testRunner.run(tests);