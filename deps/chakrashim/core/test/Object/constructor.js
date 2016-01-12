//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

Object.prototype.oString = Object.prototype.toString;
Array.prototype.oString = Object.prototype.toString;
Boolean.prototype.oString = Object.prototype.toString;
Date.prototype.oString = Object.prototype.toString;
Function.prototype.oString = Object.prototype.toString;
Number.prototype.oString = Object.prototype.toString;
RegExp.prototype.oString = Object.prototype.toString;
String.prototype.oString = Object.prototype.toString;

var x = 0;

function testEval(str) {
    eval(str);
    write(str + " x:"  + x + " typeof(x):" + typeof(x) + " x.str():" + x.oString());
}

function foo() {}

var objs = [ "undefined", "null",
            "true", "false",
            "Boolean(true)", "Boolean(false)",
            "new Boolean(true)", "new Boolean(false)",
            "NaN", "+0", "-0", "0", "0.0", "-0.0", "+0.0",
            "1", "10", "10.0", "10.1", "-1",
            "-10", "-10.0", "-10.1",
            "Number.MAX_VALUE", "Number.MIN_VALUE", "Number.NaN", "Number.POSITIVE_INFINITY", "Number.NEGATIVE_INFINITY",
            "new Number(NaN)", "new Number(+0)", "new Number(-0)", "new Number(0)",
            "new Number(0.0)", "new Number(-0.0)", "new Number(+0.0)",
            "new Number(1)", "new Number(10)", "new Number(10.0)", "new Number(10.1)", "new Number(-1)",
            "new Number(-10)", "new Number(-10.0)", "new Number(-10.1)",
            "new Number(Number.MAX_VALUE)", "new Number(Number.MIN_VALUE)", "new Number(Number.NaN)",
            "new Number(Number.POSITIVE_INFINITY)", "new Number(Number.NEGATIVE_INFINITY)",
            "''", "0xa", "04", "'hello'", "'hel' + 'lo'",
            "String('')", "String('hello')", "String('h' + 'ello')",
            "new String('')", "new String('hello')", "new String('he' + 'llo')",
            "new Object()", "new Object()",
            "[1, 2, 3]", "[1 ,2 , 3]",
            "new Array(3)", "Array(3)", "new Array(1 ,2 ,3)", "Array(1)",
            "foo"
          ];

testEval("x = Object();");
testEval("x = new Object();");

for (var i=0; i< objs.length; i++) {
    testEval("x = Object(" + objs[i] + ");");
    testEval("x = new Object(" + objs[i] + ");");
}

Object.prototype.protoFunc = function () { WScript.Echo("ObjectprotoFunc");}

var customPrototype = { protoFunc: function () { WScript.Echo("protoFunc");}}
// Constructor with only this statements
function constr(arg1, arg2)
{
    this.a = arg1;
    this.b = arg1;
}
// Constructor with more than only this statements
function constr1(arg1, arg2)
{
    if(!arg1) arg1 = 0;
    if(!arg2) arg2 = 0;
    this.a = arg1;
    this.b = arg1;
}

function body()
{
  var arr = [];
  for(var i= 0; i < 2; i++)
  {
    arr.push(new constr(1, 2, 3)); // with arg constructor cache
    arr.push(new constr());        // no arg constructor cache test

    arr.push(new constr1(1, 2, 3)); // with arg constructor cache
    arr.push(new constr1());        // no arg constructor cache test
  }
  return arr;
}

WScript.Echo("Testing no prototype property construction");
var arrayOfObjects = body();
Dump(arrayOfObjects);

WScript.Echo("Testing custom object prototype construction");
constr.prototype = customPrototype;
constr1.prototype = customPrototype;
arrayOfObjects = body();
Dump(arrayOfObjects);

WScript.Echo("Testing integer prototype construction");
constr.prototype = 1;
constr1.prototype = 1;
arrayOfObjects = body();
Dump(arrayOfObjects);

WScript.Echo("Testing no prototype property construction");
delete constr.prototype;
delete constr1.prototype;
arrayOfObjects = body();
Dump(arrayOfObjects);

function Dump(arrayOfObjects)
{
   for(var j= 0; j < arrayOfObjects.length; j++)
   {
      arrayOfObjects[j].protoFunc();
   }
}

WScript.Echo("Testing cross script context object creation");

var otherScriptContext = WScript.LoadScriptFile("constructor-crossScript.js", "samethread");

var obj = new otherScriptContext.crossContextObject();
WScript.Echo(obj.prop);
obj = new otherScriptContext.crossContextObject();
WScript.Echo(obj.prop);
obj = otherScriptContext.createObject();
WScript.Echo(obj.prop);
obj = otherScriptContext.createObject();
WScript.Echo(obj.prop);
obj = new otherScriptContext.crossContextObject();
WScript.Echo(obj.prop);

