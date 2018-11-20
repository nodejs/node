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
"This test thoroughly checks the behaviour of the 'arguments' object."
);

function access_1(a, b, c)
{
    return arguments[0];
}

function access_2(a, b, c)
{
    return arguments[1];
}

function access_3(a, b, c)
{
    return arguments[2];
}

function access_4(a, b, c)
{
    return arguments[3];
}

function access_5(a, b, c)
{
    return arguments[4];
}

function argumentLengthIs5() {
    arguments.length = 5;
    return arguments.length;
}

function duplicateArgumentAndReturnLast_call(a) {
    Array.prototype.push.call(arguments, a);
    return arguments[1];
}

function duplicateArgumentAndReturnFirst_call(a) {
    Array.prototype.push.call(arguments, a);
    return arguments[0];
}

function duplicateArgumentAndReturnLast_apply(a) {
    Array.prototype.push.apply(arguments, arguments);
    return arguments[1];
}

function duplicateArgumentAndReturnFirst_apply(a) {
    Array.prototype.push.apply(arguments, arguments);
    return arguments[0];
}

shouldBe("access_1(1, 2, 3)", "1");
shouldBe("access_2(1, 2, 3)", "2");
shouldBe("access_3(1, 2, 3)", "3");
shouldBe("access_4(1, 2, 3)", "undefined");
shouldBe("access_5(1, 2, 3)", "undefined");

shouldBe("access_1(1)", "1");
shouldBe("access_2(1)", "undefined");
shouldBe("access_3(1)", "undefined");
shouldBe("access_4(1)", "undefined");
shouldBe("access_5(1)", "undefined");

shouldBe("access_1(1, 2, 3, 4, 5)", "1");
shouldBe("access_2(1, 2, 3, 4, 5)", "2");
shouldBe("access_3(1, 2, 3, 4, 5)", "3");
shouldBe("access_4(1, 2, 3, 4, 5)", "4");
shouldBe("access_5(1, 2, 3, 4, 5)", "5");

shouldBe("argumentLengthIs5()", "5");
shouldBe("argumentLengthIs5(1,2,3,4,5)", "5");
shouldBe("argumentLengthIs5(1,2,3,4,5,6,7,8,9,10)", "5");
shouldBe("duplicateArgumentAndReturnLast_call(1)", "1");
shouldBe("duplicateArgumentAndReturnFirst_call(1)", "1");
shouldBe("duplicateArgumentAndReturnLast_apply(1)", "1");
shouldBe("duplicateArgumentAndReturnFirst_apply(1)", "1");

function f(a, b, c)
{
    return arguments;
}

function tear_off_equal_access_1(a, b, c)
{
    return f(a, b, c)[0];
}

function tear_off_equal_access_2(a, b, c)
{
    return f(a, b, c)[1];
}

function tear_off_equal_access_3(a, b, c)
{
    return f(a, b, c)[2];
}

function tear_off_equal_access_4(a, b, c)
{
    return f(a, b, c)[3];
}

function tear_off_equal_access_5(a, b, c)
{
    return f(a, b, c)[4];
}

shouldBe("tear_off_equal_access_1(1, 2, 3)", "1");
shouldBe("tear_off_equal_access_2(1, 2, 3)", "2");
shouldBe("tear_off_equal_access_3(1, 2, 3)", "3");
shouldBe("tear_off_equal_access_4(1, 2, 3)", "undefined");
shouldBe("tear_off_equal_access_5(1, 2, 3)", "undefined");

function tear_off_too_few_access_1(a)
{
    return f(a)[0];
}

function tear_off_too_few_access_2(a)
{
    return f(a)[1];
}

function tear_off_too_few_access_3(a)
{
    return f(a)[2];
}

function tear_off_too_few_access_4(a)
{
    return f(a)[3];
}

function tear_off_too_few_access_5(a)
{
    return f(a)[4];
}

shouldBe("tear_off_too_few_access_1(1)", "1");
shouldBe("tear_off_too_few_access_2(1)", "undefined");
shouldBe("tear_off_too_few_access_3(1)", "undefined");
shouldBe("tear_off_too_few_access_4(1)", "undefined");
shouldBe("tear_off_too_few_access_5(1)", "undefined");

function tear_off_too_many_access_1(a, b, c, d, e)
{
    return f(a, b, c, d, e)[0];
}

function tear_off_too_many_access_2(a, b, c, d, e)
{
    return f(a, b, c, d, e)[1];
}

function tear_off_too_many_access_3(a, b, c, d, e)
{
    return f(a, b, c, d, e)[2];
}

function tear_off_too_many_access_4(a, b, c, d, e)
{
    return f(a, b, c, d, e)[3];
}

function tear_off_too_many_access_5(a, b, c, d, e)
{
    return f(a, b, c, d, e)[4];
}

shouldBe("tear_off_too_many_access_1(1, 2, 3, 4, 5)", "1");
shouldBe("tear_off_too_many_access_2(1, 2, 3, 4, 5)", "2");
shouldBe("tear_off_too_many_access_3(1, 2, 3, 4, 5)", "3");
shouldBe("tear_off_too_many_access_4(1, 2, 3, 4, 5)", "4");
shouldBe("tear_off_too_many_access_5(1, 2, 3, 4, 5)", "5");

function live_1(a, b, c)
{
    arguments[0] = 1;
    return a;
}

function live_2(a, b, c)
{
    arguments[1] = 2;
    return b;
}

function live_3(a, b, c)
{
    arguments[2] = 3;
    return c;
}

shouldBe("live_1(0, 2, 3)", "1");
shouldBe("live_2(1, 0, 3)", "2");
shouldBe("live_3(1, 2, 0)", "3");

// Arguments that were not provided are not live
shouldBe("live_1(0)", "1");
shouldBe("live_2(1)", "undefined");
shouldBe("live_3(1)", "undefined");

shouldBe("live_1(0, 2, 3, 4, 5)", "1");
shouldBe("live_2(1, 0, 3, 4, 5)", "2");
shouldBe("live_3(1, 2, 0, 4, 5)", "3");

function extra_args_modify_4(a, b, c)
{
    arguments[3] = 4;
    return arguments[3];
}

function extra_args_modify_5(a, b, c)
{
    arguments[4] = 5;
    return arguments[4];
}

shouldBe("extra_args_modify_4(1, 2, 3, 0, 5)", "4");
shouldBe("extra_args_modify_5(1, 2, 3, 4, 0)", "5");

function tear_off_live_1(a, b, c)
{
    var args = arguments;
    return function()
    {
        args[0] = 1;
        return a;
    };
}

function tear_off_live_2(a, b, c)
{
    var args = arguments;
    return function()
    {
        args[1] = 2;
        return b;
    };
}

function tear_off_live_3(a, b, c)
{
    var args = arguments;
    return function()
    {
        args[2] = 3;
        return c;
    };
}

shouldBe("tear_off_live_1(0, 2, 3)()", "1");
shouldBe("tear_off_live_2(1, 0, 3)()", "2");
shouldBe("tear_off_live_3(1, 2, 0)()", "3");

// Arguments that were not provided are not live
shouldBe("tear_off_live_1(0)()", "1");
shouldBe("tear_off_live_2(1)()", "undefined");
shouldBe("tear_off_live_3(1)()", "undefined");

shouldBe("tear_off_live_1(0, 2, 3, 4, 5)()", "1");
shouldBe("tear_off_live_2(1, 0, 3, 4, 5)()", "2");
shouldBe("tear_off_live_3(1, 2, 0, 4, 5)()", "3");

function tear_off_extra_args_modify_4(a, b, c)
{
    return function()
    {
        arguments[3] = 4;
        return arguments[3];
    }
}

function tear_off_extra_args_modify_5(a, b, c)
{
    return function()
    {
        arguments[4] = 5;
        return arguments[4];
    }
}

shouldBe("tear_off_extra_args_modify_4(1, 2, 3, 0, 5)()", "4");
shouldBe("tear_off_extra_args_modify_5(1, 2, 3, 4, 0)()", "5");

function delete_1(a, b, c)
{
    delete arguments[0];
    return arguments[0];
}

function delete_2(a, b, c)
{
    delete arguments[1];
    return arguments[1];
}

function delete_3(a, b, c)
{
    delete arguments[2];
    return arguments[2];
}

function delete_4(a, b, c)
{
    delete arguments[3];
    return arguments[3];
}

function delete_5(a, b, c)
{
    delete arguments[4];
    return arguments[4];
}

shouldBe("delete_1(1, 2, 3)", "undefined");
shouldBe("delete_2(1, 2, 3)", "undefined");
shouldBe("delete_3(1, 2, 3)", "undefined");
shouldBe("delete_4(1, 2, 3)", "undefined");
shouldBe("delete_5(1, 2, 3)", "undefined");

shouldBe("delete_1(1)", "undefined");
shouldBe("delete_2(1)", "undefined");
shouldBe("delete_3(1)", "undefined");
shouldBe("delete_4(1)", "undefined");
shouldBe("delete_5(1)", "undefined");

shouldBe("delete_1(1, 2, 3, 4, 5)", "undefined");
shouldBe("delete_2(1, 2, 3, 4, 5)", "undefined");
shouldBe("delete_3(1, 2, 3, 4, 5)", "undefined");
shouldBe("delete_4(1, 2, 3, 4, 5)", "undefined");
shouldBe("delete_5(1, 2, 3, 4, 5)", "undefined");

function tear_off_delete_1(a, b, c)
{
    return function()
    {
        delete arguments[0];
        return arguments[0];
    };
}

function tear_off_delete_2(a, b, c)
{
    return function()
    {
        delete arguments[1];
        return arguments[1];
    };
}

function tear_off_delete_3(a, b, c)
{
    return function()
    {
        delete arguments[2];
        return arguments[2];
    };
}

function tear_off_delete_4(a, b, c)
{
    return function()
    {
        delete arguments[3];
        return arguments[3];
    };
}

function tear_off_delete_5(a, b, c)
{
    return function()
    {
        delete arguments[4];
        return arguments[4];
    };
}

shouldBe("tear_off_delete_1(1, 2, 3)()", "undefined");
shouldBe("tear_off_delete_2(1, 2, 3)()", "undefined");
shouldBe("tear_off_delete_3(1, 2, 3)()", "undefined");
shouldBe("tear_off_delete_4(1, 2, 3)()", "undefined");
shouldBe("tear_off_delete_5(1, 2, 3)()", "undefined");

shouldBe("tear_off_delete_1(1)()", "undefined");
shouldBe("tear_off_delete_2(1)()", "undefined");
shouldBe("tear_off_delete_3(1)()", "undefined");
shouldBe("tear_off_delete_4(1)()", "undefined");
shouldBe("tear_off_delete_5(1)()", "undefined");

shouldBe("tear_off_delete_1(1, 2, 3, 4, 5)()", "undefined");
shouldBe("tear_off_delete_2(1, 2, 3, 4, 5)()", "undefined");
shouldBe("tear_off_delete_3(1, 2, 3, 4, 5)()", "undefined");
shouldBe("tear_off_delete_4(1, 2, 3, 4, 5)()", "undefined");
shouldBe("tear_off_delete_5(1, 2, 3, 4, 5)()", "undefined");

function delete_not_live_1(a, b, c)
{
    delete arguments[0];
    return a;
}

function delete_not_live_2(a, b, c)
{
    delete arguments[1];
    return b;
}

function delete_not_live_3(a, b, c)
{
    delete arguments[2];
    return c;
}

shouldBe("delete_not_live_1(1, 2, 3)", "1");
shouldBe("delete_not_live_2(1, 2, 3)", "2");
shouldBe("delete_not_live_3(1, 2, 3)", "3");

shouldBe("delete_not_live_1(1)", "1");
shouldBe("delete_not_live_2(1)", "undefined");
shouldBe("delete_not_live_3(1)", "undefined");

shouldBe("delete_not_live_1(1, 2, 3, 4, 5)", "1");
shouldBe("delete_not_live_2(1, 2, 3, 4, 5)", "2");
shouldBe("delete_not_live_3(1, 2, 3, 4, 5)", "3");

function tear_off_delete_not_live_1(a, b, c)
{
    return function()
    {
        delete arguments[0];
        return a;
    };
}

function tear_off_delete_not_live_2(a, b, c)
{
    return function ()
    {
        delete arguments[1];
        return b;
    };
}

function tear_off_delete_not_live_3(a, b, c)
{
    return function()
    {
        delete arguments[2];
        return c;
    };
}

shouldBe("tear_off_delete_not_live_1(1, 2, 3)()", "1");
shouldBe("tear_off_delete_not_live_2(1, 2, 3)()", "2");
shouldBe("tear_off_delete_not_live_3(1, 2, 3)()", "3");

shouldBe("tear_off_delete_not_live_1(1)()", "1");
shouldBe("tear_off_delete_not_live_2(1)()", "undefined");
shouldBe("tear_off_delete_not_live_3(1)()", "undefined");

shouldBe("tear_off_delete_not_live_1(1, 2, 3, 4, 5)()", "1");
shouldBe("tear_off_delete_not_live_2(1, 2, 3, 4, 5)()", "2");
shouldBe("tear_off_delete_not_live_3(1, 2, 3, 4, 5)()", "3");

function access_after_delete_named_2(a, b, c)
{
    delete arguments[0];
    return b;
}

function access_after_delete_named_3(a, b, c)
{
    delete arguments[0];
    return c;
}

function access_after_delete_named_4(a, b, c)
{
    delete arguments[0];
    return arguments[3];
}

shouldBe("access_after_delete_named_2(1, 2, 3)", "2");
shouldBe("access_after_delete_named_3(1, 2, 3)", "3");
shouldBe("access_after_delete_named_4(1, 2, 3)", "undefined");

shouldBe("access_after_delete_named_2(1)", "undefined");
shouldBe("access_after_delete_named_3(1)", "undefined");
shouldBe("access_after_delete_named_4(1)", "undefined");

shouldBe("access_after_delete_named_2(1, 2, 3, 4)", "2");
shouldBe("access_after_delete_named_3(1, 2, 3, 4)", "3");
shouldBe("access_after_delete_named_4(1, 2, 3, 4)", "4");

function access_after_delete_extra_1(a, b, c)
{
    delete arguments[3];
    return a;
}

function access_after_delete_extra_2(a, b, c)
{
    delete arguments[3];
    return b;
}

function access_after_delete_extra_3(a, b, c)
{
    delete arguments[3];
    return c;
}

function access_after_delete_extra_5(a, b, c)
{
    delete arguments[3];
    return arguments[4];
}

shouldBe("access_after_delete_extra_1(1, 2, 3)", "1");
shouldBe("access_after_delete_extra_2(1, 2, 3)", "2");
shouldBe("access_after_delete_extra_3(1, 2, 3)", "3");
shouldBe("access_after_delete_extra_5(1, 2, 3)", "undefined");

shouldBe("access_after_delete_extra_1(1)", "1");
shouldBe("access_after_delete_extra_2(1)", "undefined");
shouldBe("access_after_delete_extra_3(1)", "undefined");
shouldBe("access_after_delete_extra_5(1)", "undefined");

shouldBe("access_after_delete_extra_1(1, 2, 3, 4, 5)", "1");
shouldBe("access_after_delete_extra_2(1, 2, 3, 4, 5)", "2");
shouldBe("access_after_delete_extra_3(1, 2, 3, 4, 5)", "3");
shouldBe("access_after_delete_extra_5(1, 2, 3, 4, 5)", "5");

function argumentsParam(arguments)
{
    return arguments;
}
shouldBeTrue("argumentsParam(true)");

var argumentsFunctionConstructorParam = new Function("arguments", "return arguments;");
shouldBeTrue("argumentsFunctionConstructorParam(true)");

function argumentsVarUndefined()
{
    var arguments;
    return String(arguments);
}
shouldBe("argumentsVarUndefined()", "'[object Arguments]'");

function argumentCalleeInException() {
    try {
        throw "";
    } catch (e) {
        return arguments.callee;
    }
}
shouldBe("argumentCalleeInException()", "argumentCalleeInException")

function shadowedArgumentsApply(arguments) {
    return function(a){ return a; }.apply(null, arguments);
}

function shadowedArgumentsLength(arguments) {
    return arguments.length;
}

function shadowedArgumentsCallee(arguments) {
    return arguments.callee;
}

function shadowedArgumentsIndex(arguments) {
    return arguments[0]
}

shouldBeTrue("shadowedArgumentsApply([true])");
shouldBe("shadowedArgumentsLength([])", '0');
shouldThrow("shadowedArgumentsLength()");
shouldBeUndefined("shadowedArgumentsCallee([])");
shouldBeTrue("shadowedArgumentsIndex([true])");

descriptor = (function(){ return Object.getOwnPropertyDescriptor(arguments, 1); })("zero","one","two");
shouldBe("descriptor.value", '"one"');
shouldBe("descriptor.writable", 'true');
shouldBe("descriptor.enumerable", 'true');
shouldBe("descriptor.configurable", 'true');

// Test cases for [[DefineOwnProperty]] applied to the arguments object.
(function(a0,a1,a2,a3){
    Object.defineProperties(arguments, {
        1: { get: function(){ return 201; } },
        2: { value: 202, writable: false },
        3: { writable: false },
    });

    // Test a0 is a live mapped argument.
    shouldBeTrue(String( a0 === 100 ));
    shouldBeTrue(String( arguments[0] === 100 ));
    a0 = 300;
    shouldBeTrue(String( a0 === 300 ));
    shouldBeTrue(String( arguments[0] === 300 ));
    arguments[0] = 400;
    shouldBeTrue(String( a0 === 400 ));
    shouldBeTrue(String( arguments[0] === 400 ));

    // When a1 is redefined as an accessor, it is no longer live.
    shouldBeTrue(String( a1 === 101 ));
    shouldBeTrue(String( arguments[1] === 201 ));
    a1 = 301;
    shouldBeTrue(String( a1 === 301 ));
    shouldBeTrue(String( arguments[1] === 201 ));
    arguments[1] = 401;
    shouldBeTrue(String( a1 === 301 ));
    shouldBeTrue(String( arguments[1] === 201 ));

    // When a2 is made read-only the value is set, but it is no longer live.
    // (per 10.6 [[DefineOwnProperty]] 5.b.ii.1)
    shouldBeTrue(String( a2 === 202 ));
    shouldBeTrue(String( arguments[2] === 202 ));
    a2 = 302;
    shouldBeTrue(String( a2 === 302 ));
    shouldBeTrue(String( arguments[2] === 202 ));
    arguments[2] = 402;
    shouldBeTrue(String( a2 === 302 ));
    shouldBeTrue(String( arguments[2] === 202 ));

    // When a3 is made read-only, it is no longer live.
    // (per 10.6 [[DefineOwnProperty]] 5.b.ii.1)
    shouldBeTrue(String( a3 === 103 ));
    shouldBeTrue(String( arguments[3] === 103 ));
    a3 = 303;
    shouldBeTrue(String( a3 === 303 ));
    shouldBeTrue(String( arguments[3] === 103 ));
    arguments[3] = 403;
    shouldBeTrue(String( a3 === 303 ));
    shouldBeTrue(String( arguments[3] === 103 ));

})(100,101,102,103);

// Test cases for [[DefineOwnProperty]] applied to the arguments object.
(function(arg){
    shouldBeTrue(String( Object.getOwnPropertyDescriptor(arguments, 0).writable ));
    shouldBeTrue(String( Object.getOwnPropertyDescriptor(arguments, 0).enumerable ));
    Object.defineProperty(arguments, 0, { writable: false });
    shouldBeFalse(String( Object.getOwnPropertyDescriptor(arguments, 0).writable ));
    shouldBeTrue(String( Object.getOwnPropertyDescriptor(arguments, 0).enumerable ));
    Object.defineProperty(arguments, 0, { enumerable: false });
    shouldBeFalse(String( Object.getOwnPropertyDescriptor(arguments, 0).writable ));
    shouldBeFalse(String( Object.getOwnPropertyDescriptor(arguments, 0).enumerable ));

    delete arguments[1];
    shouldBeUndefined(String( Object.getOwnPropertyDescriptor(arguments, 1) ));
    Object.defineProperty(arguments, 1, { writable: true });
    shouldBeTrue(String( Object.getOwnPropertyDescriptor(arguments, 1).writable ));
    shouldBeFalse(String( Object.getOwnPropertyDescriptor(arguments, 1).enumerable ));
})(0,1);
