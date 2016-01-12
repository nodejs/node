//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// jshost.exe -trace:perfhint -off:simplejit -maxinterpretcount:5 polymorphictest.js

var arg = "arg";
var iter = 100;
function foo1(arg1) {
    var string = "that: " + this.that1 + ", arg: " + arg1;
    return string;
}

function foo2(arg2) {
    var string = "that: " + this.that2 + ", arg: " + arg2;
    return string;
}

function foo3(arg3) {
    var string = "that: " + this.that3 + ", arg: " + arg3;
    return string;
}

function foo4(arg4) {
    var string = "that: " + this.that4 + ", arg: " + arg4;
    return string;
}

function foo5(arg5) {
    var string = "that: " + this.that5 + ", arg: " + arg5;
    return string;
}

function Test1() {
    var o1 = { foo: foo1, that1: "that1"};
    var o2 = { foo: foo2, that2: "that2"};
    var o3 = { foo: foo3, that3: "that3"};
    var o4 = { foo: foo4, that4: "that4"};
    var o5 = { foo: foo5, that5: "that5"};

    function test(obj) {
        arg += "foo";
        obj.foo(arg);
    }
    test(o1);
    test(o2);
    test(o3);
    test(o4);

    for (var i = 0; i < iter; i++) {
        result = test(o5);
    }
}

Test1();