//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

function assertTest(asserter, re, string, message) {
    asserter(re.test(string), message);
}


function assertMatches() {
    assertTest(assert.isTrue, ...arguments);
}

function assertDoesNotMatch(re, string, message) {
    assertTest(assert.isFalse, ...arguments);
}

// TODO: RegExp functions currently process strings as a list of code units as
//       opposed to a list of code points. This causes a RegExp to match just
//       the high surrogate. For example, /[^\ud800\udc00]/ matches
//       "\ud800\udc00". This this due to "\ud800" being in the negated set and
//       matching the first code unit in the string.
//
//       Some of the patterns below have the format "^...$" to force the RegExp
//       to match the string fully. Once the bug is fixes, the '^'s and '$'s
//       can be removed. The bug # is 3679792.
var tests = [
    {
        name: "A character set containing a negated character from a supplementary plane shouldn't match the character itself",
        body: function () {
            assertDoesNotMatch(/^[^\ud800\udc00]$/u, "\ud800\udc00", "Surrogate pair in RegExp and surrogate pair in string to test");
            assertDoesNotMatch(/^[^\ud800\udc00]$/u, "\u{10000}", "Surrogate pair in RegExp and code point in string to test");
            assertDoesNotMatch(/^[^\u{10000}]$/u, "\ud800\udc00", "Code point in RegExp and surrogate pair in string to test");
            assertDoesNotMatch(/^[^\u{10000}]$/u, "\u{10000}", "Code point in RegExp and code point in string to test");
        }
    },
    {
        name: "A character set containing a negated character from a supplementary plane should match other characters",
        body: function () {
            assertMatches(/^[^\ud800\udc00]$/u, "\ud801\udc01", "Surrogate pair in RegExp and surrogate pair in string to test");
            assertMatches(/^[^\u{10000}]$/u, "\ud801\udc01", "Surrogate pair in RegExp and code point in string to test");
            assertMatches(/^[^\ud800\udc00]$/u, "\u{10101}", "Code point in RegExp and surrogate pair in string to test");
            assertMatches(/^[^\u10000]$/u, "\u{10101}", "Code point in RegExp and code point in string to test");

            assertMatches(/^[^\u10000]$/u, "\u0345", "Code point in RegExp and code unit in string to test");
            assertMatches(/^[^\ud800\udc00]$/u, "\u0345", "Surrorgate pair in RegExp and code unit in string to test");
        }
    },
    {
        name: "A character set containing a negated character from the basic plane should match characters from supplementary planes",
        body: function () {
            assertMatches(/^[^0345]$/u, "\ud800\udc00", "Surrogate pair");
            assertMatches(/^[^0345]$/u, "\u{10000}", "Code point");
        }
    },
    {
        name: "A character set containing a range spanning multiple planes should match characters from all those planes",
        body: function () {
            var re = /^[\u0000-\u{10FFFF}]$/u;

            var numberOfPlanes = 17;
            for (var plane = 0; plane < numberOfPlanes; ++plane) {
                function getCharacterInPlane(code) {
                    var codePoint = plane * 0x10000 + code;
                    return String.fromCodePoint(codePoint);
                }

                assertMatches(re, getCharacterInPlane(0x0000), "First character in plane #" + plane);
                assertMatches(re, getCharacterInPlane(0xFFFF), "Last character in plane #" + plane);
            }
        }
    },
    {
        name: "A dash character and a non-dash character following a full one shouldn't be interpreted as a range",
        body: function () {
            var re = /^[\ud800-\udbff\udc00-\udbff\udc02]$/u;

            assertDoesNotMatch(re, "\udbff\udc01", "Shouldn't be in the second range");
            assertMatches(re, "-", "Second '-' treated as a normal character");
        }
    },
    {
        name: "Reserved characters shouldn't be ignored when they are in a character set together with characters from a supplementary plane",
        body: function () {
            assertMatches(/^[\ud800\udc00 \ud800]$/u, "\ud800", "Start of the reserver character range (\\ud800)");
            assertMatches(/^[\ud800\udc00 \udfff]$/u, "\udfff", "Start of the reserver character range (\\udfff)");
        }
    },
    {
        name: "A high and a low surrogate part with a '-' between should be interpreted as a range",
        body: function () {
            assertMatches(/^[\ud800-\udfff]$/u, "\ud800", "Range start");
            assertMatches(/^[\ud800-\udfff]$/u, "\udfff", "Range end");

            // We had a bug where we interpreted the character set below as [\ud800\udfff] and omitted '-'.
            assertDoesNotMatch(/^[\ud800-\udfff]$/u, "\ud800\udfff", "Not a surrogate pair");
        }
    }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
