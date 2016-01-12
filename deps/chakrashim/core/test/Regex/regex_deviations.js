//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

if (this.WScript && this.WScript.LoadScriptFile) {
    this.WScript.LoadScriptFile('..\\UnitTestFramework\\UnitTestFramework.js');
}

var tests = [
    {
        name: '"\\c" should be treated as "\\\\c" outside the character set',
        assertions: [
            [/^\c$/, '\\c'],
        ]
    },
    {
        name: '"\\c" should be treated as "c" inside the character set',
        assertions: [
            [/[\c]/, 'c'],
            [/[\c-]/, 'c']
        ]
    },
    {
        name: 'A non-letter character after "\\c" inside the character set should be the letter\'s mathematical value mod 32',
        assertions: [
            [/[\c1]/, '\x11']
        ]
    },
    {
        name: 'A non-letter character after "\\c" outside the character set should not be treated differently',
        assertions: [
            [/\c1/, '\\c1']
        ]
    },
    {
        name: '"]" should be allowed on its own',
        assertions: [
            [/]/, ']']
        ]
    }
];

var testsForRunner = tests.map(function (test) {
    return {
        name: test.name,
        body: function () {
            test.assertions.forEach(function (assertion) {
                var re = assertion[0];
                var inputString = assertion[1];
                assert.isTrue(re.test(inputString), re.source);
            });
        }
    }
});

testRunner.runTests(testsForRunner, { verbose: WScript.Arguments[0] != 'summary' });
