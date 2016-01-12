//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var o = new Object();

WScript.Echo("*** Setting data property ***");
o.x = 23;
WScript.Echo("o.x=" + o.x);

WScript.Echo("*** Setting data property using defineProperty ***");
Object.defineProperty(o, "x", { value : 24 });
WScript.Echo("o.x=" + o.x);

WScript.Echo("*** Setting accessor property using defineProperty ***");
var x = "";
var z = "";
Object.defineProperty(o, "x", {
    get : function() { WScript.Echo("Getter called"); return x; },
    set : function(val) { WScript.Echo("Setter called"); z = 1000; x = val; }
});
o.x = 25;
WScript.Echo("o.x=" + o.x);
WScript.Echo("x=" + x);
WScript.Echo("z=" + z);
WScript.Echo("*** Setting backing store for accessor ***");
x = 26;
WScript.Echo("o.x=" + o.x);

var ab = new Object();
Object.defineProperty(ab,"foo",{get:function(){WScript.Echo("In getter");}, configurable: true});
Object.defineProperty(ab,"foo",{set:function(arg){WScript.Echo("In setter");}});
ab.foo;
ab.foo = 10;

delete ab.foo;

try
{
 var ab = new Object();
 Object.defineProperty(ab,"foo",{get:function(){WScript.Echo("In getter");}});
 ab.foo;
 ab.foo = 10;
}
catch(e)
{
  WScript.Echo(e.description);
}

delete ab.foo;

try
{
 var ab = new Object();
 Object.defineProperty(ab,"foo",{set:function(arg){WScript.Echo("In setter");}});
 WScript.Echo(ab.foo);
 ab.foo = 10;
}
catch(e)
{
  WScript.Echo(e.description);
}

delete ab.foo;

var o = {};

o.a = 1;
o.b = 2;
o.c = 3;
o.d = 4;
o.e = 5;
o.f = 6;
o.g = 7;
o.h = 8;
o.i = 9;
o.j = 10;
o.k = 11;
o.l = 12;
o.m = 13;
o.n = 14;
o.o = 15;
o.p = 16;
o.q = 17;

Object.defineProperty(o, "qqq",
    {
        set: function () { },
        get: function() { WScript.Echo("get"); }
    });

WScript.Echo(o.qqq);
delete o.qqq;

// prototype setter/getter

function Point() {
    this.x=0;
    this.y=0;
}

Point.prototype = {
    print:function() { WScript.Echo("x:"+this.x+", y:"+this.y+", z:"+this.z); }
};

Object.defineProperty(Point.prototype,"z",{ set:function(v) { this._z=v; }, get: function() { return this._z; }});
var pt=new Point();
pt.z=12;
pt.print();

Object.defineProperty(this, "abc",
    {
        set: function () { },
        get: function() { WScript.Echo("get global"); }
    });
WScript.Echo(abc);
delete this.abc;

(function () {
    WScript.Echo("*** Getters, prototypes, and deleting properties ***");

    function A() { };
    A.prototype = {
        get p () { return this._p; },
        set p (v) { this._p = v; }
    };
    var o = new A();
    o.p;
    delete A.prototype.p;
    o.p;
    WScript.Echo(o.p);
    WScript.Echo(A.prototype.p);
})();

(function () {
    WScript.Echo("*** Setters, prototypes, and deleting properties ***");

    function A() { };
    A.prototype = {
        get p () { return this._p; },
        set p (v) { this._p = v; }
    };
    var o = new A();
    o._p = undefined; // create the property to stop the setter from changing the type
    o.p = 1;
    delete A.prototype.p;
    o.p = 2;
    WScript.Echo(o.p);
    WScript.Echo(A.prototype.p);
})();
