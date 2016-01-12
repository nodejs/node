//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function Verify(expression, actualValue, expectedValue)
{
    if (expectedValue != actualValue)
    {
        write("Failed: " + expression + ". Exp:" + expectedValue + " Act:" + actualValue);
    }
    else
    {
        write("PASS: " + expression + ":" + expectedValue);
    }
}

function PropertyExists(obj, propName)
{
    return obj.hasOwnProperty(propName);
}

function check(value)
{
    write(value);
}

function IncrVal()
{
    check("IncrVal:: " + this.val + " args.length : " + arguments.length);
    this.val++;
    return this.val + " " + arguments.length;
}

// Global Variables
var val = 100;
var fGlobalThis;


// Tests

//-----------------------------------------------------------------------------------------------------------
// Global Object tests

fGlobalThis = IncrVal.bind();

Verify("global object", fGlobalThis(),      "101 0");
Verify("global object", fGlobalThis(10),    "102 1");
Verify("global object", fGlobalThis(10,20), "103 2");

val = 100;
fGlobalThis = IncrVal.bind(this);

Verify("global object", fGlobalThis(),      "101 0");
Verify("global object", fGlobalThis(10),    "102 1");
Verify("global object", fGlobalThis(10,20), "103 2");

val = 100;
fGlobalThis = IncrVal.bind(this, 50);

Verify("global object", fGlobalThis(),      "101 1");
Verify("global object", fGlobalThis(10),    "102 2");
Verify("global object", fGlobalThis(10,20), "103 3");

val = 100;
fGlobalThis = IncrVal.bind(this, 50, 51);

Verify("global object", fGlobalThis(),      "101 2");
Verify("global object", fGlobalThis(10),    "102 3");
Verify("global object", fGlobalThis(10,20), "103 4");

//-----------------------------------------------------------------------------------------------------------


function testGlobalBinding()
{
    check("Testing Bind to global object");

    val = 100;
    var f1 = IncrVal.bind();

    Verify("GlobalObject length", f1.length, 0);

    Verify("global object", f1(),      "101 0");
    Verify("global object", f1(10),    "102 1");
    Verify("global object", f1(10,20), "103 2");
}

function testLocalBinding()
{
    check("Testing Bind to local object");

    var objWithVal = { val : 200 }
    var fLocal = IncrVal.bind(objWithVal);

    Verify("Local length", fLocal.length, 0);

    Verify("Local object1", fLocal(),      "201 0");
    Verify("Local object2", fLocal(10),    "202 1");
    Verify("Local object3", fLocal(10,20), "203 2");

    objWithVal = { val : 200 }
    fLocal = IncrVal.bind(objWithVal, 50);

    Verify("local length", fLocal.length, 0);

    Verify("local object", fLocal(),      "201 1");
    Verify("local object", fLocal(10),    "202 2");
    Verify("local object", fLocal(10,20), "203 3");

    objWithVal = { val : 200 }
    fLocal = IncrVal.bind(objWithVal, 50, 51);

    Verify("local length", fLocal.length, 0);
    Verify("local object", fLocal(),      "201 2");
    Verify("local object", fLocal(10),    "202 3");
    Verify("local object", fLocal(10,20), "203 4");
}

function testBoundLength()
{
    check("Testing Length");

    var obj = new Object();

    function f0() { }
    function f1(x1) { }
    function f2(x1,x2) { }
    function f3(x1,x2,x3) { }
    function f4(x1,x2,x3,x4) { }
    function f5(x1,x2,x3,x4,x5) { }

    var bf0 = f0.bind(); Verify("1 Length0 ", bf0.length, 0);
    var bf1 = f1.bind(); Verify("1 Length1 ", bf1.length, 1);
    var bf2 = f2.bind(); Verify("1 Length2 ", bf2.length, 2);
    var bf3 = f3.bind(); Verify("1 Length3 ", bf3.length, 3);
    var bf4 = f4.bind(); Verify("1 Length4 ", bf4.length, 4);
    var bf5 = f5.bind(); Verify("1 Length5 ", bf5.length, 5);

    bf0 = f0.bind(obj); Verify("2 Length0 ", bf0.length, 0);
    bf1 = f1.bind(obj); Verify("2 Length1 ", bf1.length, 1);
    bf2 = f2.bind(obj); Verify("2 Length2 ", bf2.length, 2);
    bf3 = f3.bind(obj); Verify("2 Length3 ", bf3.length, 3);
    bf4 = f4.bind(obj); Verify("2 Length4 ", bf4.length, 4);
    bf5 = f5.bind(obj); Verify("2 Length5 ", bf5.length, 5);

    bf0 = f0.bind(obj, 10); Verify("3 Length0 ", bf0.length, 0);
    bf1 = f1.bind(obj, 10); Verify("3 Length1 ", bf1.length, 0);
    bf2 = f2.bind(obj, 10); Verify("3 Length2 ", bf2.length, 1);
    bf3 = f3.bind(obj, 10); Verify("3 Length3 ", bf3.length, 2);
    bf4 = f4.bind(obj, 10); Verify("3 Length4 ", bf4.length, 3);
    bf5 = f5.bind(obj, 10); Verify("3 Length5 ", bf5.length, 4);
}

function testConstruction()
{
    check("Testing Construction");

    function sum(a,b) { this.r = a + b; return this;}

    var obj = new Object();
    var add10 = sum.bind(obj, 10);
    var res = new add10(20);

    Verify("Construction ", res.r, 30);
}

var x = 20;
var y = 30;

function testProto()
{
    function add()
    {
        return this.x + this.y;
    }

    Verify("Add Test", add(), 50);

    var o = { x: 5, y: 6};
    var f = add.bind(o);

    Verify("f Test", f(), 11);

    var f2 = new f();

    Verify("Proto Test", add.prototype.isPrototypeOf(f2), true);
}

function testConstruction1()
{
    function Point(x,y)
    {
        this.x = x;
        this.y = y;
    }

    var p0 = Point.bind();
    var r0 = new p0(0, 1);
    Verify("TestConstruction0 x", r0.x, 0);
    Verify("TestConstruction0 y", r0.y, 1);

    var o1 = new Object();
    var p1 = Point.bind(o1);
    var r1 = new p1(100, 101);
    Verify("TestConstruction1 x", r1.x, 100);
    Verify("TestConstruction1 y", r1.y, 101);

    var o2 = new Object();
    var p2 = Point.bind(o2, 200);
    var r2 = new p2(201);
    Verify("TestConstruction2 x", r2.x, 200);
    Verify("TestConstruction2 y", r2.y, 201);

    var o3 = new Object();
    var p3 = Point.bind(o3, 300, 301);
    var r3 = new p3();
    Verify("TestConstruction3 x", r3.x, 300);
    Verify("TestConstruction3 y", r3.y, 301);
}

testGlobalBinding();
testLocalBinding();
testBoundLength(); 
testConstruction();
testProto();
testConstruction1();
