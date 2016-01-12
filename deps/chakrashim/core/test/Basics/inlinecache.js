//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function Test1() {
    write(Math.PI);
    write(Math.PI);
    Math.PI++;
    write(Math.PI);
    write(Math.PI);
}

Test1();

function Test2() {
    var a = [ 10, 20]

    write(a.concat());
    write(a.concat());
}

Test2();

function Test3() {

    function f() { write("in f"); }

    var o = {};
    Object.defineProperty(o, "x", { writable: false, value: f });

    write(o.x);
    o.x();
    o.x();
    write(o.x);
}

Test3();

function Test4() {

 Object.defineProperty(this, "x", ({get: function(){}}));
 eval("for(var i=0;i< 10; i++ ) {x=20;}");

}
Test4();
