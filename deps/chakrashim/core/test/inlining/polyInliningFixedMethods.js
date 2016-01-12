//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function a() { }
a.prototype.x = function () { WScript.Echo(1); this.y(); };
a.prototype.y = function () { WScript.Echo("a"); };
var a1 = new a();
a1.f = function () { WScript.Echo(1); this.y(); };

function b() { }
b.prototype.p = 1;
b.prototype.x = function () { WScript.Echo("b"); };
b.prototype.f = function () { WScript.Echo("b1"); };
b.prototype.y = function () { WScript.Echo("b"); };
var b1 = new b();

function pr(){};
pr.f = function() {WScript.Echo("pr")};
b.prototype = pr;
b2 = new b();

function Inheriter() {
    this.f = function () { WScript.Echo("f"); }
}

function c() { }
c.prototype.x = function () { WScript.Echo(2); };
c.prototype.q = 1;
c.prototype.p = 1;

Inheriter.prototype = a.prototype;
c.prototype = new Inheriter();

var c1 = new c();

function d() { }
d.prototype.x = function () { WScript.Echo(2); };
d.prototype.r = 1;
d.prototype.q = 1;
d.prototype.p = 1;

//Inheriter.prototype = b.prototype;
d.prototype = new Inheriter();

var d1 = new d();

function foo(func) {
    func.f();
}

foo(b1);
foo(b2);
foo(b1);
foo(d1);

foo(a1);
foo(b1);
foo(c1);
foo(d1);