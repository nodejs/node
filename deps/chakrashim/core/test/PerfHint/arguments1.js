//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// jshost -trace:perfhint d:\testbins1\test.js -off:simplejit -maxinterpretcount:1

var target = function (arg) {
    var string = "that: " + this.that +
                 ", arg: " + arg;
    return string;
};

var that = { that: "that" };

function arguments_test1(arg1) {
    target.apply(that, arguments);
}

function arguments_test1_fixed() {
    target.apply(that, arguments);
}

function arguments_test2() {
    var k = 10;
    arguments[arguments.length] = 'end';
    target.apply(that, arguments);
}

function arguments_test3() {
    var arr = [];
    for (var i in arguments) {
        arr.push(arguments[i]);
    }

    arr.push('end');
    target.apply(that, arr);
}

function arguments_test2_fixed() {
    var k = arguments.length;
    var arr = [];
    for (var i = 0; i < k; i++) {
        arr[i] = arguments[i];
    }

    arr.push('end');
    target.apply(that, arr);
}

var arg = "arg";
var iter = 100;

function Run() {
    for (var i = 0; i < iter; i++) {
        arguments_test1(arg);
    }
    for (var i = 0; i < iter; i++) {
        arguments_test1_fixed(arg);
    }
    for (var i = 0; i < iter; i++) {
        arguments_test2(arg);
    }
    for (var i = 0; i < iter; i++) {
        arguments_test3(arg);
    }
    for (var i = 0; i < iter; i++) {
        arguments_test2_fixed(arg);
    }
}
Run();
