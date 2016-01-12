//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var i;

Object.prototype.u ="o.p.u";
Object.prototype.x ="o.p.x";
Object.prototype.y = "o.p.y";
Object.prototype.z = "o.p.z";

var f1 = function(){};
f1.prototype.x = "f.p.x";
f1.prototype.q = "f.p.q";
f1.prototype.z = "f.p.z";
f1.prototype.r = "f.p.r";

var a1 = new f1();

a1.x = "a.x";
a1.q = "a.q";
a1.u = "a.q";

for (i in a1)
{
  WScript.Echo(i+":"+a1[i]);
}

var a = new Object();
a.x="hello";
a.y="world";

var o  = new foo();
o.pqr = "pqr";

WScript.Echo("Object a");

for (i in a)
{
    WScript.Echo(i);
}

WScript.Echo("Math");

for (i in Math)
{
    WScript.Echo(i);
}


WScript.Echo("Array");

for (i in Array)
{
    WScript.Echo(i);
}

WScript.Echo("Array.prototype");

for (i in Array.prototype)
{
    WScript.Echo(i);
}

WScript.Echo("Date");
for (i in Date)
{
    WScript.Echo(i);
}

WScript.Echo("Number");
for (i in Number)
{
    WScript.Echo(i);
}

WScript.Echo("String");
for (i in String)
{
    WScript.Echo(i);
}


WScript.Echo("Object.prototype");

Object.prototype.z = "me too";

for (i in Object.prototype)
{
    WScript.Echo(i);
}

WScript.Echo("Object");

for (i in Object)
{
    WScript.Echo(i);
}

WScript.Echo("Array.prototype.sort");

for(i in Array.prototype.sort)
{
  WScript.Echo(i);
}


WScript.Echo("function foo");

function foo()
{
  this.xyz  = "xyz";
}


for(i in foo)
{
  WScript.Echo(i);
}

Array.prototype.sort.x = "hello"
var o = Array.prototype.sort;

for (i in Array.prototype.sort)
{
  WScript.Echo(i);
}

WScript.Echo("me here");


WScript.Echo("prototype chain");

Object.prototype.x = 10;

function f() { }
function g() { }

g.prototype = new f();

y = new g();

for (i in y) { WScript.Echo(i); }

var aString = "StringType";

String.prototype.zz = "s.p.zz";

var bString = new String("StringObject");
bString.xx = "bString.xx";
bString.yy = "bString.yy";


WScript.Echo("Literal String");
for( i in aString) { WScript.Echo(i); }

WScript.Echo("String Object");
for( i in bString) { WScript.Echo(i); }


function Person(){}
Person.prototype[5]=20;
var a = new Person();
for (var i in a) { WScript.Echo(i); }


Array.prototype[3] = 3;
var a = new Array();
for (var i in a) { WScript.Echo(i); }

for ( i in null ) { WScript.Echo(i); }
