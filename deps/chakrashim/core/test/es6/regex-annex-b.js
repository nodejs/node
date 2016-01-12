//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

function withCharacterSet(assertions) {
    var newAssertions = [];
    assertions.forEach(function (assertion) {
        newAssertions.push(assertion);

        var pattern = assertion[0];
        var rest = assertion.slice(1);
        var characterSetAssertion = ['[' + pattern + ']'].concat(rest);
        newAssertions.push(characterSetAssertion);
    });
    return newAssertions;
}

var tests = [
    {
        name: 'Lookahead assertions should be quantifiable',
        assertions: [
            ['(?=a)+ab', 'ab'],
            ['(?!a)+b', 'b'],
        ]
    },
    {
        name: 'Curly braces should be allowed on their own',
        assertions: withCharacterSet([
            ['{', '{'],
            ['}', '}']
        ])
    },
    {
        name: 'Legacy octal escape sequences should be allowed',
        assertions: withCharacterSet([
            ['\\0', '\x00'],
            ['\\7', '\x07'],
            ['\\00', '\x00'],
            ['\\37', '\x1F'],
            ['\\41', '\x21'],
            ['\\77', '\x3F'],
            ['\\000', '\x00'],
            ['\\377', '\xFF'],
        ])
    },
    {
        name: '"\\8" and "\\9" should be identity escapes',
        assertions: withCharacterSet([
            ['\\8', '8'],
            ['\\9', '9'],
        ])
    },
    {
        name: 'Legacy octal escape sequence should be disambiguated based on the number of groups',
        assertions: [
            ['(1)(2)\\1\\2\\3', '1212\x03'],
        ]
    },
    {
        name: '"\\x" should be treated as an identity escape if ill-formed',
        assertions: withCharacterSet([
            ['\\x', 'x'],
            ['\\xa', 'xa'],
        ])
    },
    {
        name: '"\\u" should be treated as an identity escape if ill-formed',
        assertions: withCharacterSet([
            ['\\u', 'u'],
            ['\\ua', 'ua'],
        ])
    },
    {
        name: 'Any character except "c" should be escapable',
        assertions: withCharacterSet([
            ['\\q', 'q'],
        ])
    },
];

var testsForRunner = tests.map(function (test) {
    return {
        name: test.name,
        body: function () {
            test.assertions.forEach(function (assertion) {
                var pattern = assertion[0];
                var inputString = assertion[1];

                var re = eval('/' + pattern + '/');
                assert.isTrue(re.test(inputString), re.source);
            });
        }
    }
});

testRunner.runTests(testsForRunner, { verbose: WScript.Arguments[0] != 'summary' });
