//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function f1() 
{
    this.x1 = "hello";
}

f1.apply();
write("x1 : " + x1);

x1 = 0;
f1.apply(null);
write("x1 : " + x1);

x1 = 0;
f1.apply(undefined);
write("x1 : " + x1);

var o = new Object();

x1 = 0;
f1.apply(o);
write("x1 : " + x1);
write("o.x1 : " + o.x1);

function f2(a)
{
    this.x2 = a;
}

x2 = 0;
f2.apply();
write("x2 : " + x2);

x2 = 0;
f2.apply(null);
write("x2 : " + x2);

x2 = 0;
f2.apply(undefined);
write("x2 : " + x2);

x2 = 0;
f2.apply(o);
write("x2 : " + x2);
write("o.x2 : " + o.x2);

x2 = 0;
f2.apply(null, ["world"]);
write("x2 : " + x2);

x2 = 0;
f2.apply(undefined, ["world"]);
write("x2 : " + x2);

x2 = 0;
f2.apply(o, ["world"]);
write("x2 : " + x2);
write("o.x2 : " + o.x2);


function blah()
{
    this.construct.apply(this, arguments);
    write("after apply");
    return new Object();
}

function blah2()
{
    try
    {
        this.construct.apply(this, arguments);
    }
    catch (e)
    {
    }
    
    write("after apply");
    return new Object();
}

blah.prototype.construct = function(x, y)
{
    this.a = x;
    this.b = y;
}
blah2.prototype.construct = function(x, y)
{
    this.a = x;
    this.b = y;
}

var o = new blah(1, 2);
write(o.a);
write(o.b);

o = new blah2(1, 2);

write(o.a);
write(o.b);

function f() {}

f.apply({},{});
f.apply({},{length:null});
f.apply({},{length:undefined});
f.apply({},{length:0.5});
