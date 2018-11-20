// Copyright 2013 the V8 project authors. All rights reserved.
// Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

description(
"This test checks that toString() round-trip on a function that has prefix, postfix and typeof operators applied to group expression will not remove the grouping. Also checks that evaluation of such a expression produces run-time exception"
);

/* These have become obsolete, since they are not syntactically well-formed ES5+.

function postfix_should_preserve_parens(x, y, z) {
    (x, y)++;
    return y;
}

function prefix_should_preserve_parens(x, y, z) {
    ++(x, y);
    return x;

}

function both_should_preserve_parens(x, y, z) {
    ++(x, y)--;
    return x;

}

function postfix_should_preserve_parens_multi(x, y, z) {
    (((x, y)))--;
    return x;
}

function prefix_should_preserve_parens_multi(x, y, z) {
    --(((x, y)));
    return x;
}

function both_should_preserve_parens_multi(x, y, z) {
    ++(((x, y)))--;
    return x;
}

function postfix_should_preserve_parens_multi1(x, y, z) {
    (((x)), y)--;
    return x;
}

function prefix_should_preserve_parens_multi1(x, y, z) {
    --(((x)), y);
    return x;
}

function prefix_should_preserve_parens_multi2(x, y, z) {
    var z = 0;
    --(((x), y), z);
    return x;
}

function postfix_should_preserve_parens_multi2(x, y, z) {
    var z = 0;
    (((x), y) ,z)++;
    return x;
}
*/

// if these return a variable (such as y) instead of
// the result of typeof, this means that the parenthesis
// got lost somewhere.
function typeof_should_preserve_parens(x, y, z) {
    return typeof (x, y);
}

function typeof_should_preserve_parens1(x, y, z) {
    return typeof ((x, y));
}

function typeof_should_preserve_parens2(x, y, z) {
    var z = 33;
    return typeof (z, (x, y));
}

function typeof_should_preserve_parens_multi(x, y, z) {
    var z = 33;
    return typeof ((z,(((x, y)))));
}

unevalf = function(x) { return '(' + x.toString() + ')'; };

function testToString(fn) {
    // check that toString result evaluates to code that can be evaluated
    // this doesn't actually reveal the bug that this test is testing
    shouldBe("unevalf(eval(unevalf("+fn+")))", "unevalf(" + fn + ")");

    // check that grouping operator is still there (this test reveals the bug
    // but will create possible false negative if toString output changes in
    // the future)
    shouldBeTrue("/.*\\(+x\\)*, y\\)/.test(unevalf("+fn+"))");

}

function testToStringAndRTFailure(fn)
{
    testToString(fn);

    // check that function call produces run-time exception
    shouldThrow(""+fn+ "(1, 2, 3);");

    // check that function call produces run-time exception after eval(unevalf)
    shouldThrow("eval(unevalf("+fn+ "))(1, 2, 3);");
}

function testToStringAndReturn(fn, p1, p2, retval)
{

    testToString(fn);

    // check that function call produces correct result
    shouldBe("" + fn + "(" + p1 + ", " + p2 +");", retval);

    // check that function call produces correct result after eval(unevalf)
    shouldBe("eval(unevalf("+fn+ "))" + "(" + p1 + ", " + p2 +");", retval);
}


/*
testToStringAndRTFailure("prefix_should_preserve_parens");
testToStringAndRTFailure("postfix_should_preserve_parens");
testToStringAndRTFailure("both_should_preserve_parens");
testToStringAndRTFailure("prefix_should_preserve_parens_multi");
testToStringAndRTFailure("postfix_should_preserve_parens_multi");
testToStringAndRTFailure("prefix_should_preserve_parens_multi1");
testToStringAndRTFailure("postfix_should_preserve_parens_multi1");
testToStringAndRTFailure("prefix_should_preserve_parens_multi2");
testToStringAndRTFailure("postfix_should_preserve_parens_multi2");
*/

testToStringAndReturn("typeof_should_preserve_parens", "'a'", 1, "'number'");
testToStringAndReturn("typeof_should_preserve_parens1", "'a'", 1, "'number'");
testToStringAndReturn("typeof_should_preserve_parens2", "'a'", 1, "'number'");
testToStringAndReturn("typeof_should_preserve_parens_multi", "'a'", 1, "'number'");
