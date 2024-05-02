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
'Tests whether bytecode codegen properly handles temporaries.'
);

var a = true;
a = false || a;
shouldBeTrue("a");

var b = false;
b = true && b;
shouldBeFalse("b");

function TestObject() {
    this.toString = function() { return this.test; }
    this.test = "FAIL";
    return this;
}

function assign_test1()
{
    var testObject = new TestObject;
    var a = testObject;
    a.test = "PASS";
    return testObject.test;
}

shouldBe("assign_test1()", "'PASS'");

function assign_test2()
{
    var testObject = new TestObject;
    var a = testObject;
    a = a.test = "PASS";
    return testObject.test;
}

shouldBe("assign_test2()", "'PASS'");

function assign_test3()
{
    var testObject = new TestObject;
    var a = testObject;
    a.test = a = "PASS";
    return testObject.test;
}

shouldBe("assign_test3()", "'PASS'");

var testObject4 = new TestObject;
var a4 = testObject4;
a4.test = this.a4 = "PASS";

shouldBe("testObject4.test", "'PASS'");

var testObject5 = new TestObject;
var a5 = testObject5;
a5 = this.a5.test = "PASS";

shouldBe("testObject5.test", "'PASS'");

function assign_test6()
{
    var testObject = new TestObject;
    var a = testObject;
    a["test"] = "PASS";
    return testObject.test;
}

shouldBe("assign_test6()", "'PASS'");

function assign_test7()
{
    var testObject = new TestObject;
    var a = testObject;
    a = a["test"] = "PASS";
    return testObject.test;
}

shouldBe("assign_test7()", "'PASS'");

function assign_test8()
{
    var testObject = new TestObject;
    var a = testObject;
    a["test"] = a = "PASS";
    return testObject.test;
}

shouldBe("assign_test8()", "'PASS'");

function assign_test9()
{
    var testObject = new TestObject;
    var a = testObject;
    a["test"] = this.a = "PASS";
    return testObject.test;
}

shouldBe("assign_test9()", "'PASS'");

var testObject10 = new TestObject;
var a10 = testObject10;
a10 = this.a10["test"] = "PASS";

shouldBe("testObject10.test", "'PASS'");

function assign_test11()
{
    var testObject = new TestObject;
    var a = testObject;
    a[a = "test"] = "PASS";
    return testObject.test;
}

shouldBe("assign_test11()", "'PASS'");

function assign_test12()
{
    var test = "test";
    var testObject = new TestObject;
    var a = testObject;
    a[test] = "PASS";
    return testObject.test;
}

shouldBe("assign_test12()", "'PASS'");

function assign_test13()
{
    var testObject = new TestObject;
    var a = testObject;
    a.test = (a = "FAIL", "PASS");
    return testObject.test;
}

shouldBe("assign_test13()", "'PASS'");

function assign_test14()
{
    var testObject = new TestObject;
    var a = testObject;
    a["test"] = (a = "FAIL", "PASS");
    return testObject.test;
}

shouldBe("assign_test14()", "'PASS'");

function assign_test15()
{
    var test = "test";
    var testObject = new TestObject;
    var a = testObject;
    a[test] = (test = "FAIL", "PASS");
    return testObject.test;
}

shouldBe("assign_test15()", "'PASS'");

function assign_test16()
{
    var a = 1;
    a = (a = 2);
    return a;
}

shouldBe("assign_test16()", "2");

var a17 = 1;
a17 += (a17 += 1);

shouldBe("a17", "3");

function assign_test18()
{
    var a = 1;
    a += (a += 1);
    return a;
}

shouldBe("assign_test18()", "3");

var a19 = { b: 1 };
a19.b += (a19.b += 1);

shouldBe("a19.b", "3");

function assign_test20()
{
    var a = { b: 1 };
    a.b += (a.b += 1);
    return a.b;
}

shouldBe("assign_test20()", "3");

var a21 = { b: 1 };
a21["b"] += (a21["b"] += 1);

shouldBe("a21['b']", "3");

function assign_test22()
{
    var a = { b: 1 };
    a["b"] += (a["b"] += 1);
    return a["b"];
}

shouldBe("assign_test22()", "3");

function assign_test23()
{
    var o = { b: 1 };
    var a = o;
    a.b += a = 2;
    return o.b;
}

shouldBe("assign_test23()", "3");

function assign_test24()
{
    var o = { b: 1 };
    var a = o;
    a["b"] += a = 2;
    return o["b"];
}

shouldBe("assign_test24()", "3");

function assign_test25()
{
    var o = { b: 1 };
    var a = o;
    a[a = "b"] += a = 2;
    return o["b"];
}

shouldBe("assign_test25()", "3");

function assign_test26()
{
    var o = { b: 1 };
    var a = o;
    var b = "b";
    a[b] += a = 2;
    return o["b"];
}

shouldBe("assign_test26()", "3");

function assign_test27()
{
    var o = { b: 1 };
    var a = o;
    a.b += (a = 100, 2);
    return o.b;
}

shouldBe("assign_test27()", "3");

function assign_test28()
{
    var o = { b: 1 };
    var a = o;
    a["b"] += (a = 100, 2);
    return o["b"];
}

shouldBe("assign_test28()", "3");

function assign_test29()
{
    var o = { b: 1 };
    var a = o;
    var b = "b";
    a[b] += (a = 100, 2);
    return o["b"];
}

shouldBe("assign_test29()", "3");

function assign_test30()
{
    var a = "foo";
    a += (a++);
    return a;
}

shouldBe("assign_test30()", "'fooNaN'");

function assign_test31()
{
    function result() { return "PASS"; }
    return (globalVar = result)()
}

shouldBe("assign_test31()", "'PASS'");

function bracket_test1()
{
    var o = [-1];
    var a = o[++o];
    return a;
}

shouldBe("bracket_test1()", "-1");

function bracket_test2()
{
    var o = [1];
    var a = o[--o];
    return a;
}

shouldBe("bracket_test2()", "1");

function bracket_test3()
{
    var o = [0];
    var a = o[o++];
    return a;
}

shouldBe("bracket_test3()", "0");

function bracket_test4()
{
    var o = [0];
    var a = o[o--];
    return a;
}

shouldBe("bracket_test4()", "0");

function bracket_test5()
{
    var o = [1];
    var a = o[o ^= 1];
    return a;
}

shouldBe("bracket_test5()", "1");

function bracket_test6()
{
    var o = { b: 1 }
    var b = o[o = { b: 2 }, "b"];
    return b;
}

shouldBe("bracket_test6()", "1");

function mult_test1()
{
    var a = 1;
    return a * (a = 2);
}

shouldBe("mult_test1()", "2");

function mult_test2()
{
    var a = 1;
    return a * ++a;
}

shouldBe("mult_test2()", "2");

function mult_test3()
{
    var a = 1;
    return a * (a += 1);
}

shouldBe("mult_test3()", "2");

function div_test1()
{
    var a = 1;
    return a / (a = 2);
}

shouldBe("div_test1()", "0.5");

function div_test2()
{
    var a = 1;
    return a / ++a;
}

shouldBe("div_test2()", "0.5");

function div_test3()
{
    var a = 1;
    return a / (a += 1);
}

shouldBe("div_test3()", "0.5");

function mod_test1()
{
    var a = 1;
    return a % (a = 2);
}

shouldBe("mod_test1()", "1");

function mod_test2()
{
    var a = 1;
    return a % ++a;
}

shouldBe("mod_test2()", "1");

function mod_test3()
{
    var a = 1;
    return a % (a += 1);
}

shouldBe("mod_test3()", "1");

function add_test1()
{
    var a = 1;
    return a + (a = 2);
}

shouldBe("add_test1()", "3");

function add_test2()
{
    var a = 1;
    return a + ++a;
}

shouldBe("add_test2()", "3");

function add_test3()
{
    var a = 1;
    return a + (a += 1);
}

shouldBe("add_test3()", "3");

function sub_test1()
{
    var a = 1;
    return a - (a = 2);
}

shouldBe("sub_test1()", "-1");

function sub_test2()
{
    var a = 1;
    return a - ++a;
}

shouldBe("sub_test2()", "-1");

function sub_test3()
{
    var a = 1;
    return a - (a += 1);
}

shouldBe("sub_test3()", "-1");

function lshift_test1()
{
    var a = 1;
    return a << (a = 2);
}

shouldBe("lshift_test1()", "4");

function lshift_test2()
{
    var a = 1;
    return a << ++a;
}

shouldBe("lshift_test2()", "4");

function lshift_test3()
{
    var a = 1;
    return a << (a += 1);
}

shouldBe("lshift_test3()", "4");

function rshift_test1()
{
    var a = 4;
    return a >> (a = 2);
}

shouldBe("rshift_test1()", "1");

function rshift_test2()
{
    var a = 2;
    return a >> --a;
}

shouldBe("rshift_test2()", "1");

function rshift_test3()
{
    var a = 2;
    return a >> (a -= 1);
}

shouldBe("rshift_test3()", "1");

function urshift_test1()
{
    var a = 4;
    return a >>> (a = 2);
}

shouldBe("urshift_test1()", "1");

function urshift_test2()
{
    var a = 2;
    return a >>> --a;
}

shouldBe("urshift_test2()", "1");

function urshift_test3()
{
    var a = 2;
    return a >>> (a -= 1);
}

shouldBe("urshift_test3()", "1");

function less_test1()
{
    var a = 1;
    return a < (a = 2);
}

shouldBeTrue("less_test1()");

function less_test2()
{
    var a = 1;
    return a < ++a;
}

shouldBeTrue("less_test2()");

function less_test3()
{
    var a = 1;
    return a < (a += 1);
}

shouldBeTrue("less_test3()");

function greater_test1()
{
    var a = 2;
    return a > (a = 1);
}

shouldBeTrue("greater_test1()");

function greater_test2()
{
    var a = 2;
    return a > --a;
}

shouldBeTrue("greater_test2()");

function greater_test3()
{
    var a = 2;
    return a > (a -= 1);
}

shouldBeTrue("greater_test3()");

function lesseq_test1()
{
    var a = 1;
    return a <= (a = 3, 2);
}

shouldBeTrue("lesseq_test1()");

function lesseq_test2()
{
    var a = 1;
    return a <= (++a, 1);
}

shouldBeTrue("lesseq_test2()");

function lesseq_test3()
{
    var a = 1;
    return a <= (a += 1, 1);
}

shouldBeTrue("lesseq_test3()");

function greatereq_test1()
{
    var a = 2;
    return a >= (a = 1, 2);
}

shouldBeTrue("greatereq_test1()");

function greatereq_test2()
{
    var a = 2;
    return a >= (--a, 2);
}

shouldBeTrue("greatereq_test2()");

function greatereq_test3()
{
    var a = 2;
    return a >= (a -= 1, 2);
}

shouldBeTrue("greatereq_test3()");

function instanceof_test1()
{
    var a = { };
    return a instanceof (a = 1, Object);
}

shouldBeTrue("instanceof_test1()");

function instanceof_test2()
{
    var a = { valueOf: function() { return 1; } };
    return a instanceof (++a, Object);
}

shouldBeTrue("instanceof_test2()");

function instanceof_test3()
{
    var a = { valueOf: function() { return 1; } };
    return a instanceof (a += 1, Object);
}

shouldBeTrue("instanceof_test3()");

function in_test1()
{
    var a = "a";
    return a in (a = "b", { a: 1 });
}

shouldBeTrue("in_test1()");

function in_test2()
{
    var a = { toString: function() { return "a"; }, valueOf: function() { return 1; } };
    return a in (++a, { a: 1 });
}

shouldBeTrue("in_test2()");

function in_test3()
{
    var a = { toString: function() { return "a"; }, valueOf: function() { return 1; } };
    return a in (a += 1, { a: 1 });
}

shouldBeTrue("in_test3()");

function eq_test1()
{
    var a = 1;
    return a == (a = 2);
}

shouldBeFalse("eq_test1()");

function eq_test2()
{
    var a = 1;
    return a == ++a;
}

shouldBeFalse("eq_test2()");

function eq_test3()
{
    var a = 1;
    return a == (a += 1);
}

shouldBeFalse("eq_test3()");

function neq_test1()
{
    var a = 1;
    return a != (a = 2);
}

shouldBeTrue("neq_test1()");

function neq_test2()
{
    var a = 1;
    return a != ++a;
}

shouldBeTrue("neq_test2()");

function neq_test3()
{
    var a = 1;
    return a != (a += 1);
}

shouldBeTrue("neq_test3()");

function stricteq_test1()
{
    var a = 1;
    return a === (a = 2);
}

shouldBeFalse("stricteq_test1()");

function stricteq_test2()
{
    var a = 1;
    return a === ++a;
}

shouldBeFalse("stricteq_test2()");

function stricteq_test3()
{
    var a = 1;
    return a === (a += 1);
}

shouldBeFalse("stricteq_test3()");

function nstricteq_test1()
{
    var a = 1;
    return a !== (a = 2);
}

shouldBeTrue("nstricteq_test1()");

function nstricteq_test2()
{
    var a = 1;
    return a !== ++a;
}

shouldBeTrue("nstricteq_test2()");

function nstricteq_test3()
{
    var a = 1;
    return a !== (a += 1);
}

shouldBeTrue("nstricteq_test3()");

function bitand_test1()
{
    var a = 1;
    return a & (a = 2);
}

shouldBe("bitand_test1()", "0");

function bitand_test2()
{
    var a = 1;
    return a & ++a;
}

shouldBe("bitand_test2()", "0");

function bitand_test3()
{
    var a = 1;
    return a & (a += 1);
}

shouldBe("bitand_test3()", "0");

function bitor_test1()
{
    var a = 1;
    return a | (a = 2);
}

shouldBe("bitor_test1()", "3");

function bitor_test2()
{
    var a = 1;
    return a | ++a;
}

shouldBe("bitor_test2()", "3");

function bitor_test3()
{
    var a = 1;
    return a | (a += 1);
}

shouldBe("bitor_test3()", "3");

function bitxor_test1()
{
    var a = 1;
    return a ^ (a = 2);
}

shouldBe("bitxor_test1()", "3");

function bitxor_test2()
{
    var a = 1;
    return a ^ ++a;
}

shouldBe("bitxor_test2()", "3");

function bitxor_test3()
{
    var a = 1;
    return a ^ (a += 1);
}

shouldBe("bitxor_test3()", "3");

function switch_test1_helper(a, b)
{
    switch (a) {
    case b:
        break;
    default:
        break;
    }

    return b;
}

function switch_test1()
{
    return switch_test1_helper(0, 1) == 1;
}

shouldBeTrue("switch_test1()");

function switch_test2_helper(a, b)
{
    var c = b;
    switch (a) {
    case c:
        break;
    default:
        break;
    }

    return c;
}

function switch_test2()
{
    return switch_test2_helper(0, 1) == 1;
}

shouldBeTrue("switch_test2()");

function switch_test3_helper(a)
{
    switch (a) {
    case this:
        break;
    default:
        break;
    }

    return this;
}

function switch_test3()
{
    return this == switch_test3_helper.call(this, 0);
}

shouldBeTrue("switch_test3()");

function construct_test()
{
    var c = [function(a) { this.a = a; }];

    function f()
    {
        return new c[0](true);
    }

    return f().a;
}

shouldBeTrue("construct_test()");
var testStr = "[";
for (var i = 0; i < 64; i++)
    testStr += "(0/0), ";
testStr += "].length";
shouldBe(testStr, "64");
