//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function X()
{
    this.x = 1;
}
function Y(s)
{
    this[s] = 2;
}
function Z()
{
    this.z = 3;
}

Y.prototype = new Z();
X.prototype = new Y("y");

var x = new X();
var y = new Y("yy");
var z = new Z();

with(x)
{
    WScript.Echo("x.x = " + x);
    WScript.Echo("x.y = " + y);
    WScript.Echo("x.z = " + z);

    ++z;

    WScript.Echo("x.z = " + z);

    // refers to x.y
    with(y)
    {
        WScript.Echo("x.x = " + x);
        WScript.Echo("x.y = " + y);
        WScript.Echo("x.z = " + z);
    }

    y = new Object();

    y.m = 7;

    // refers to x.y
    with(y)
    {
        WScript.Echo("x.y.m = " + m);
    }

    y = undefined;

    if(y == undefined)
    {
        WScript.Echo("OK: y in with scope is undefined");
    }

    Z.prototype.zz = 1;

    WScript.Echo("x.zz = " + zz);

    // get rid of x.x
    x = undefined;

    if(x == undefined)
    {
        WScript.Echo("OK: x in with scope is undefined");
    }
}

with(Z.prototype)
{
    zz *= 10;
    with(Z)
    {
        prototype.zz++;

        with(prototype)
        {
            zz *= 100;
        }
    }
}

var q = new Y("a");

with(x)
{
    WScript.Echo("x.x = " + x);
    WScript.Echo("x.y = " + y);
    WScript.Echo("x.z = " + z);
    WScript.Echo("x.zz = " + zz);
}

with(q) { with(q) { with(q) {

    WScript.Echo("q.a = " + a);
    WScript.Echo("q.zz = " + zz);

}}}

(function () {
    function a()
    {
        WScript.Echo("a is called");
    }

    (function(){
        try {
            throw a;
        }
        catch(x) {
            with({}){
                x();
            }
        }
    })();
})();

(function () {
    var o = {};
    var y = function x(){
        with(o){
            x(o.x = function(){});
        }
    };

    y();
})();
