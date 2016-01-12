//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

var obj = [];
var proto_1 = [];
var proto_2 = [];
var count = 2;

function Ctor1()
{
    this.x = 0;
    this.y = 1;
}

function Ctor2() {
    this.a = 0;
    this.b = 1;
}

function test(o1, o2, ctor1, ctor2)
{
    var isO1Ctor1 = o1 instanceof ctor1;
    var isO2Ctor1 = o2 instanceof ctor1;
    write("o1 instanceof ctor1: " + isO1Ctor1);
    write("o2 instanceof ctor1: " + isO2Ctor1);
}

var o1 = new Ctor1();
var o2 = new Ctor2();
test(o1, o2, Ctor1, Ctor2);
Ctor1.prototype = { x: 10, y: 20 };
test(o1, o2, Ctor1, Ctor2);
