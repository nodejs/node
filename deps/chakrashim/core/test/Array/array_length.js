//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(args)
{
  WScript.Echo(args);
}

var arr=new Array(2);
arr[0]=1;
arr.length="";
write(arr.length);

arr.length=null;
write(arr.length);

arr.length=4294967295;
write(arr.length.toString());

try {
    arr.length="-1";
}
catch (e)
{
    write(e.message);
}

try {
    arr.length=4294967296;
}
catch (e)
{
    write(e.message);
}

try {
    Array.length=10;
    write(Array.length);
}
catch (e)
{
    write(e.message);
}

try {
    x = [];
    x.length = true;
    write(x.length);
}
catch(e)
{
    write(e.message);
}

try {
    Object.prototype.length = function () {
        return this
    };
    (function () {
        ;
        for (var y in [void 0]) {
            y.length();
        }
    })();
}
catch (e) {
    write(e.message);
}

try {
    Object.prototype.length = function () {
        return this
    };
    var a = [10, 20, 24];
    WScript.Echo("prop = " + a.length);
    WScript.Echo("method = " + a.length());
}
catch (e) {
    write(e.message);
}

var a = { length: 10 };
var b = Object.freeze(a);
var c = Object.create(b);
c.length = 88;
WScript.Echo(c.length);
WScript.Echo(b.length);

var o = Object.freeze([]);
var p = Object.create(o)
p.length = 5
WScript.Echo(p.length);
WScript.Echo(o.length);

var x = [];
var y = Object.create(x);
y.length = 7;
WScript.Echo(y.length);
WScript.Echo(x.length);

var z = [];
z.length = 3;
WScript.Echo(z.length);

function echo(m) { if (this.WScript) { WScript.Echo(m); } else { console.log(m); } }
Object.defineProperty(Object.prototype, "length", { set: function() { echo("setter"); }, configurable: true });
var a = [];
var b = Object.create(a);
b.length = 5;
echo(b.length);

function foo()
{
    var arr = new Array(10);
    var x = arr.length--;
    arr[arr.length + 1] = 20;
    var y = --arr.length;
    return y;
}
WScript.Echo(foo());

