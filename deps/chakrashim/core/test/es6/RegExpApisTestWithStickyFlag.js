//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

function createMessage(message, prefix) {
    prefix = (prefix != undefined) ? prefix + ": " : "";
    return prefix + message;
}

function assertSplit(re, messagePrefix) {
    var str = "a,ab,ba,b,";
    var result = str.split(re);
    assert.areEqual(3, result.length, createMessage("Sticky = true, RegExp.split() result's length", messagePrefix));
    assert.areEqual("", result[0], createMessage("Sticky = true, RegExp.split() result[0]", messagePrefix));
    assert.areEqual("ab,b", result[1], createMessage("Sticky = true, RegExp.split() result[1]", messagePrefix));
    assert.areEqual("b,", result[2], createMessage("Sticky = true, RegExp.split() result[2]", messagePrefix));
    assert.areEqual(0, re.lastIndex, createMessage("Sticky = true, lastIndex result on RegExp.split()", messagePrefix));
}

function assertSplitWithSingleCharacterPattern(re, messagePrefix) {
    var str = "abaca";
    var result = str.split(re);
    assert.areEqual(4, result.length, createMessage("Sticky = true, RegExp.split() result's length", messagePrefix));
    assert.areEqual("", result[0], createMessage("Sticky = true, RegExp.split() result", messagePrefix));
    assert.areEqual("b", result[1], createMessage("Sticky = true, RegExp.split() result", messagePrefix));
    assert.areEqual("c", result[2], createMessage("Sticky = true, RegExp.split() result", messagePrefix));
    assert.areEqual("", result[3], createMessage("Sticky = true, RegExp.split() result", messagePrefix));
    assert.areEqual(0, re.lastIndex, createMessage("Sticky = true, lastIndex result on RegExp.split()", messagePrefix));
}

function createReplaceValue(replaceValueType) {
    return replaceValueType === "string" ? "1" : function () { return "1"; };
}

var tests = [
  {
    name: "RegExp.test() - matches for the beginning of string, otherwise terminates if sticky = true",
    body: function () {
        var str = "abcababc";
        var re = /abc/y;
        assert.isTrue(re.test(str), "Sticky = true, RegExp.test() result");
        assert.isTrue(re.lastIndex == 3, "Sticky = true, lastIndex result on RegExp.test()");
        assert.isFalse(re.test(str), "Sticky = true, RegExp.test() result");
        assert.isTrue(re.lastIndex == 0, "Sticky = true, lastIndex result on RegExp.test()");
        assert.isTrue(re.test(str), "Sticky = true, RegExp.test() result");
        assert.isTrue(re.lastIndex == 3, "Sticky = true, lastIndex result on RegExp.test()");
    }
  },
  {
    name: "RegExp.exec() - matches for the beginning of string, otherwise terminates if sticky = true",
    body: function () {
        var str = "abcababc";
        var re = /abc/y;
        assert.isTrue(re.exec(str) == "abc", "Sticky = true, RegExp.exec() result");
        assert.isTrue(re.lastIndex == 3, "Sticky = true, lastIndex result on RegExp.exec()");
        assert.isTrue(re.exec(str) == null, "Sticky = true, RegExp.exec() result");
        assert.isTrue(re.lastIndex == 0, "Sticky = true, lastIndex result on RegExp.exec()");
    }
  },
  {
    name: "RegExp.match() - matches for the beginning of string, otherwise terminates if sticky = true",
    body: function () {
        var str = "abcababc";
        var re = /abc/y;
        assert.isTrue(str.match(re) == "abc", "Sticky = true, RegExp.match() result");
        assert.isTrue(re.lastIndex == 3, "Sticky = true, lastIndex result on RegExp.match()");
        assert.isTrue(str.match(re) == null, "Sticky = true, RegExp.match() result");
        assert.isTrue(re.lastIndex == 0, "Sticky = true, lastIndex result on RegExp.match()");
    }
  },
  {
    name: "RegExp.match() - matches for the beginning of string, otherwise terminates if sticky = true with lastindex set",
    body: function () {
        var str = "abcabcababc";
        var re = /abc/y;
        re.lastIndex = 3;
        assert.isTrue(str.match(re) == "abc", "Sticky = true, RegExp.match() result");
        assert.isTrue(re.lastIndex == 6, "Sticky = true, lastIndex result on RegExp.match()");
        assert.isTrue(str.match(re) == null, "Sticky = true, RegExp.match() result");
        assert.isTrue(re.lastIndex == 0, "Sticky = true, lastIndex result on RegExp.match()");
    }
  },
  {
    name: "RegExp.search() - matches for the beginning of string, otherwise terminates if sticky = true",
    body: function () {
        var str = "ababcabc";
        var re = /abc/y;
        assert.isTrue(str.search(re) == -1, "Sticky = true, RegExp.search() result");
        assert.isTrue(re.lastIndex == 0, "Sticky = true, lastIndex result on RegExp.search()");
        assert.isTrue(str.search(re) == -1, "Sticky = true, RegExp.search() result");
        assert.isTrue(re.lastIndex == 0, "Sticky = true, lastIndex result on RegExp.search()");
    }
  },
  {
    name: "RegExp.replace() - matches for the beginning of string, otherwise terminates if sticky = true",
    body: function () {
        function assertReplace(replaceValueType) {
            var str = "abcabcababc";
            var re = /abc/y;
            var replaceValue = createReplaceValue(replaceValueType);
            assert.isTrue(str.replace(re, replaceValue) == "1abcababc", "Sticky = true, replaceValue type = " + replaceValueType + ", RegExp.replace() result");
            assert.isTrue(re.lastIndex == 3, "Sticky = true, replaceValue type = " + replaceValueType + ", lastIndex result on RegExp.replace()");
            assert.isTrue(str.replace(re, replaceValue) == "abc1ababc", "Sticky = true, replaceValue type = " + replaceValueType + ", RegExp.replace() result");
            assert.isTrue(re.lastIndex == 6, "Sticky = true, replaceValue type = " + replaceValueType + ", lastIndex result on RegExp.replace()");
            assert.isTrue(str.replace(re, replaceValue) == "abcabcababc", "Sticky = true, replaceValue type = " + replaceValueType + ", RegExp.replace() result");
            assert.isTrue(re.lastIndex == 0, "Sticky = true, replaceValue type = " + replaceValueType + ", lastIndex result on RegExp.replace()");
        }

        ["string", "function"].forEach(assertReplace);

    }
  },
  {
    name: "RegExp.replace() - matches for the beginning of string, otherwise terminates if sticky = true, lastIndex set",
    body: function () {
        function assertReplace(replaceValueType) {
            var str = "abcabcababc";
            var re = /abc/y;
            re.lastIndex = 4;
            var replaceValue = createReplaceValue(replaceValueType);
            assert.isTrue(str.replace(re, replaceValue) == "abcabcababc", "Sticky = true, replaceValue type = " + replaceValueType + ", RegExp.replace() result");
            assert.isTrue(re.lastIndex == 0, "Sticky = true, replaceValue type = " + replaceValueType + ", lastIndex result on RegExp.replace()");
        }

        ["string", "function"].forEach(assertReplace);
    }
  },
  {
    name: "RegExp.replace() - matches for the beginning of string, otherwise terminates if sticky = true, global = true",
    body: function () {
        function assertReplace(replaceValueType) {
            var str = "abcabcababc";
            var re = /abc/gy;
            var replaceValue = createReplaceValue(replaceValueType);
            assert.isTrue(str.replace(re, replaceValue) == "11ababc", "Sticky = true, replaceValue type = " + replaceValueType + ", RegExp.replace() result");
            assert.isTrue(re.lastIndex == 0, "Sticky = true, replaceValue type = " + replaceValueType + ", lastIndex result on RegExp.replace()");
            assert.isTrue(str.replace(re, replaceValue) == "11ababc", "Sticky = true, replaceValue type = " + replaceValueType + ", RegExp.replace() result");
            assert.isTrue(re.lastIndex == 0, "Sticky = true, replaceValue type = " + replaceValueType + ", lastIndex result on RegExp.replace()");
        }

        ["string", "function"].forEach(assertReplace);
    }
  },
  {
    name: "RegExp.replace() - matches for the beginning of string, otherwise terminates if global = true",
    body: function () {
        function assertReplace(replaceValueType) {
            var str = "abcabcababc";
            var re = /abc/g;
            var replaceValue = createReplaceValue(replaceValueType);
            assert.isTrue(str.replace(re, replaceValue) == "11ab1", "Sticky = true, replaceValue type = " + replaceValueType + ", RegExp.replace() result");
            assert.isTrue(re.lastIndex == 0, "Sticky = true, replaceValue type = " + replaceValueType + ", lastIndex result on RegExp.replace()");
            assert.isTrue(str.replace(re, replaceValue) == "11ab1", "Sticky = true, replaceValue type = " + replaceValueType + ", RegExp.replace() result");
            assert.isTrue(re.lastIndex == 0, "Sticky = true, replaceValue type = " + replaceValueType + ", lastIndex result on RegExp.replace()");
        }

        ["string", "function"].forEach(assertReplace);
    }
  },
  {
    name: "RegExp.replace() - returns the input string as it is when lastIndex >= input length",
    body: function () {
        function assertReplace(replaceValueType) {
            var str = "abc";
            var re = /a/y;
            var lastIndex = str.length;
            re.lastIndex = lastIndex;
            var replaceValue = createReplaceValue(replaceValueType);

            var result = str.replace(re, replaceValue);

            var messageBase = "Input length: " + str.length + ", lastIndex = " + lastIndex + ", replaceValue type = " + replaceValueType;
            var message = messageBase + ", result";
            assert.areEqual(str, result, message);
            var message = messageBase + ", lastIndex after replace()";
            assert.areEqual(0, re.lastIndex, message);
        }

        ["string", "function"].forEach(assertReplace);
    }
  },
  {
    name: "RegExp.split() - ignores sticky flag if created via literal",
    body: function () {
        var re = /a,/y;
        assertSplit(re);
    }
  },
  {
    name: "RegExp.split() - ignores sticky flag if created via constructor",
    body: function () {
        var re = new RegExp("a,", "y");
        assertSplit(re, "Non-RegExp pattern");

        var re2 = new RegExp(re);
        assertSplit(re2, "RegExp pattern, 'flags' aren't present");

        var re3 = new RegExp(re, "y", "RegExp pattern, 'flags' are present");
        assertSplit(re3);

        var re4 = new RegExp("A,", "y");
        "asd".split(re);
        var re5 = new RegExp(re4, "i");
        assertSplit(re2, "Changed flags");
    }
  },
  {
    name: "RegExp.split() - ignores sticky flag if created via RegExp.prototype.compile",
    body: function () {
        var re = /./.compile("a,", "y");
        assertSplit(re, "Non-RegExp pattern");

        var re2 = /./.compile(re);
        assertSplit(re2, "RegExp pattern");
    }
  },
  {
    name: "RegExp.split() - ignores sticky flag if single-character pattern",
    body: function () {
        var reCaseSensitive = /a/y;
        assertSplitWithSingleCharacterPattern(reCaseSensitive, "Case-sensitive");

        var reCaseInsensitive = /A/iy;
        assertSplitWithSingleCharacterPattern(reCaseInsensitive, "Case-insensitive");
    }
  },
  {
    name: "RegExp.split() - result is propagated to the constructor",
    body: function () {
        var str = "abac";
        var re = /(a[c]?)/y;
        var result = str.split(re);

        // If the actual result is "a", that means we propagated "re"s original pattern
        // as opposed to the separate one for split().
        assert.areEqual("ac", RegExp.$1);
    }
  },
  {
    name: "RegExp.split() - ignores lastIndex",
    body: function () {
        var re = /a,/y;
        re.lastIndex = 3;
        assertSplit(re);
    }
  }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
