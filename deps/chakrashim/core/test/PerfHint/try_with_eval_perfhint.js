//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo1() {
    return new Date();
}

function foo() {
    try {
        function bar() {
            return 10;
        }
        bar();
        foo1();
    }
    catch (e) {
    }
}
foo();
foo();
foo();

function foo2() {
    var k = 0;
    try {
        k = foo1();
    }
    finally {
        k = "";
    }
}
foo2();
foo2();
foo2();

function foo3() {
    function foo4() {
        eval('foo4');
    }
    foo4();
}
foo3();
foo3();
foo3();

function foo5() {
    var obj = { x: 10 };
    with (obj) {
        function foo6() {
            x = 31;
        }
    }
    foo6();
}
foo5();
foo5();
foo5();
