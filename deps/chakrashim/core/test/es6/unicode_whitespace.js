//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 unicode whitespace tests

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var whitespace_characters = [
    '\u0009',
    '\u000a',
    '\u000b',
    '\u000c',
    '\u000d',
    '\u0020',
    '\u00a0',
    '\u1680',
    '\u180e',
    '\u2000',
    '\u2001',
    '\u2002',
    '\u2003',
    '\u2004',
    '\u2005',
    '\u2006',
    '\u2007',
    '\u2008',
    '\u2009',
    '\u200a',
    '\u2028',
    '\u2029',
    '\u202f',
    '\u205f',
    '\u3000',
    '\ufeff',
];

// These characters may not be printable but are not considered whitespace.
// See ES6 Section 11.2 White Space
var special_non_whitespace_characters = [
    '\u0085',
    '\u200b',
    '\u200c',
    '\u200d',
    '\u2060',
    '\u00b7',
    '\u237d',
    '\u2420',
    '\u2422',
    '\u2423',
];

var tests = [
    {
        name: "String#trim correctly removes whitespace characters",
        body: function () {
            for (idx in whitespace_characters) {
                var ch = whitespace_characters[idx];
                var result = ch.trim();

                assert.areEqual(0, result.length, "String#trim removes whitespace characters, result should have 0 length");
                assert.areEqual("", result, "String#trim removes whitespace characters, result should be empty string");
            }
        }
    },
    {
        name: "String#trim correctly removes all whitespace characters",
        body: function () {
            var str = whitespace_characters.join('');
            var result = str.trim();

            assert.areEqual(0, result.length, "String#trim removes whitespace characters, result should have 0 length");
            assert.areEqual("", result, "String#trim removes whitespace characters, result should be empty string");
        }
    },
    {
        name: "String#trim correctly ignores special non-whitespace characters",
        body: function () {
            for (idx in special_non_whitespace_characters) {
                var ch = special_non_whitespace_characters[idx];
                var result = ch.trim();

                assert.areEqual(1, result.length, "String#trim leaves non-whitespace characters, result should 1 character long");
                assert.areEqual(ch, result, "String#trim leaves non-whitespace characters, result should be equal to the input");
            }
        }
    },
    {
        name: "String#trim correctly ignores all non-whitespace characters",
        body: function () {
            for (var hex = 0x0000; hex <= 0xffff; hex++) {
                var ch = String.fromCodePoint(hex);
                var result = ch.trim();

                if (result.length === 0) {
                    var found = false;
                    for (idx in whitespace_characters) {
                        if (ch === whitespace_characters[idx]) {
                            found = true;
                        }
                    }

                    assert.isTrue(found, "If we found a whitespace character, it had to be one of the known whitespace characters");
                } else {
                    assert.areEqual(ch, result, "If the character we found is not a whitespace character, the trimmed string has to be equal to the character itself");
                }
            }
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
