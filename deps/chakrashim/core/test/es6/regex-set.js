//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var normalTests = [
    {
        regExp: /[-]/u,
        acceptedCharacters: ['-'],
        rejectedCharacters: [',', '.']
    },
    {
        regExp: /[b-]/u,
        acceptedCharacters: ['b', '-'],
        rejectedCharacters: ['a', 'c']
    },
    {
        regExp: /[\u{10001}-]/u,
        acceptedCharacters: ['\u{10001}', '-'],
        rejectedCharacters: ['\u{10000}', '\u{10002}']
    },
    {
        regExp: /[-b]/u,
        acceptedCharacters: ['-', 'b'],
        rejectedCharacters: ['a', 'c']
    },
    {
        regExp: /[-\u{10001}]/u,
        acceptedCharacters: ['-', '\u{10001}'],
        rejectedCharacters: ['\u{10000}', '\u{10002}']
    },
    {
        regExp: /[b-d]/u,
        acceptedCharacters: ['b', 'c', 'd'],
        rejectedCharacters: ['-', 'a', 'e']
    },
    {
        regExp: /[\u{10001}-\u{10003}]/u,
        acceptedCharacters: ['\u{10001}', '\u{10002}', '\u{10003}'],
        rejectedCharacters: ['-', '\u{10000}', '\u{10004}']
    },
    {
        regExp: /[+--]/u,
        acceptedCharacters: ['+', ',', '-'],
        rejectedCharacters: ['*', '.']
    },
    {
        regExp: /[--/]/u,
        acceptedCharacters: ['-', '.', '/'],
        rejectedCharacters: [',', '0']
    },
    {
        regExp: /[b-d-]/u,
        acceptedCharacters: ['b', 'c', 'd', '-'],
        rejectedCharacters: ['a', 'e']
    },
    {
        regExp: /[\u{10001}-\u{10003}-]/u,
        acceptedCharacters: ['\u{10001}', '\u{10002}', '\u{10003}', '-'],
        rejectedCharacters: ['\u{10000}', '\u{10004}']
    },
    {
        regExp: /[b-df]/u,
        acceptedCharacters: ['b', 'c', 'd', 'f'],
        rejectedCharacters: ['-', 'a', 'e']
    },
    {
        regExp: /[\u{10001}-\u{10003}\u{10005}]/u,
        acceptedCharacters: ['\u{10001}', '\u{10002}', '\u{10003}', '\u{10005}'],
        rejectedCharacters: ['-', '\u{10000}', '\u{10004}']
    },
    {
        regExp: /[b-df-]/u,
        acceptedCharacters: ['b', 'c', 'd', 'f', '-'],
        rejectedCharacters: ['a', 'e', 'g']
    },
    {
        regExp: /[\u{10001}-\u{10003}\u{10005}-]/u,
        acceptedCharacters: ['\u{10001}', '\u{10002}', '\u{10003}', '\u{10005}', '-'],
        rejectedCharacters: ['\u{10000}', '\u{10004}', '\u{10006}']
    },
    {
        // Third non-dash character's mathematical value is higher than the second's
        regExp: /[b-d-f]/u,
        acceptedCharacters: ['b', 'c', 'd', '-', 'f'],
        rejectedCharacters: ['a', 'e']
    },
    {
        // Third non-dash character's mathematical value is higher than the second's
        regExp: /[\u{10001}-\u{10003}-\u{10005}]/u,
        acceptedCharacters: ['\u{10001}', '\u{10002}', '\u{10003}', '-', '\u{10005}'],
        rejectedCharacters: ['\u{10000}', '\u{10004}']
    },
    {
        // Third non-dash character's mathematical value is lower than the second's
        regExp: /[c-e-a]/u,
        acceptedCharacters: ['c', 'd', 'e', '-', 'a'],
        rejectedCharacters: ['b', 'f']
    },
    {
        // Third non-dash character's mathematical value is lower than the second's
        regExp: /[\u{10002}-\u{10004}-\u{10000}]/u,
        acceptedCharacters: ['\u{10002}', '\u{10003}', '\u{10004}', '-', '\u{10000}'],
        rejectedCharacters: ['\u{10001}', '\u{10005}']
    },
    {
        regExp: /[b-df-h]/u,
        acceptedCharacters: ['b', 'c', 'd', 'f', 'g', 'h'],
        rejectedCharacters: ['-', 'a', 'e', 'i']
    },
    {
        regExp: /[\u{10001}-\u{10003}\u{10005}-\u{10007}]/u,
        acceptedCharacters: ['\u{10001}', '\u{10002}', '\u{10003}', '\u{10005}', '\u{10006}', '\u{10007}'],
        rejectedCharacters: ['-', '\u{10000}', '\u{10004}', '\u{10008}']
    },
    {
        regExp: /[b-d\u{10001}-\u{10003}]/u,
        acceptedCharacters: ['b', 'c', 'd', '\u{10001}', '\u{10002}', '\u{10003}'],
        rejectedCharacters: ['-', 'a', 'e', '\u{10000}', '\u{10004}']
    },
    {
        // Make sure we don't omit the \u{10400}-\u{107FF} range
        regExp: /[\u{10001}-\u{10BFE}]/u,
        acceptedCharacters: ['\u{10001}', '\u{103FF}', '\u{10400}', '\u{107FF}', '\u{10800}', '\u{10BFE}']
    },
    {
        regExp: /[\u{10000}-\u{107FF}]/u,
        acceptedCharacters: ['\u{10000}', '\u{103FF}', '\u{10400}', '\u{107FF}']
    },
    {
        // Make sure we don't omit the \u{10400}-\u{107FF} range
        regExp: /[\u{10000}-\u{10802}]/u,
        acceptedCharacters: ['\u{10000}', '\u{103FF}', '\u{10400}', '\u{107FF}', '\u{10800}', '\u{10802}']
    }
];

var testsForRunner = normalTests.map(function (test) {
    return {
        name: '' + test.regExp + ' should work correctly',
        body: function () {
            function makePrintable(ch) {
                var charCode = ch.charCodeAt(0);
                if ('a'.charCodeAt(0) <= charCode && charCode <= 'z'.charCodeAt(0)
                    || 'A'.charCodeAt(0) <= charCode && charCode <= 'Z'.charCodeAt(0)
                    || '0'.charCodeAt(0) <= charCode && charCode <= '9'.charCodeAt(0)
                    || ch === '-') {
                    return ch;
                }
                else {
                    var hexString = ch.codePointAt(0).toString(16);
                    return '\\u{' + hexString + '}';
                }
            }

            function createMessage(ch, result) {
                return "'" + makePrintable(ch) + "' should be " + result;
            }

            test.acceptedCharacters.forEach(function (ch) {
                assert.isTrue(test.regExp.test(ch), createMessage(ch, 'acceptedCharacters'));
            });

            rejectedCharacters = test.rejectedCharacters || [];
            rejectedCharacters.forEach(function (ch) {
                assert.isFalse(test.regExp.test(ch), createMessage(ch, 'rejectedCharacters'));
            });
        }
    };
});

var disallowedPatterns = [
    '/[b-a]/',
    '/[\\u{10001}-\\u{10000}]/u',
];
testsForRunner = testsForRunner.concat(disallowedPatterns.map(function (pattern) {
    return {
        name: '' + pattern + ' should throw SyntaxError',
        body: function () {
            assert.throws(function () { eval(pattern); }, SyntaxError);
        }
    };
}));

testRunner.runTests(testsForRunner, { verbose: WScript.Arguments[0] != 'summary' });
