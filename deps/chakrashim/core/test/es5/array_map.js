//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function returnValueSquare(x,y,z)
{
    WScript.Echo("value:"+ x + " index:" + y + " Object:" + z);
    return x*x;
}

function returnIndexSquare(x,y,z)
{
    WScript.Echo("value:"+ x + " index:" + y + " Object:" + z);
    return y*y;
}

function returnRandom(x,y,z)
{
    WScript.Echo("value:"+ x + " index:" + y + " Object:" + z);
    return x*y;
}

Array.prototype[6] = 20;

var x = [1,2,3,4,5];
var y = x.map(returnValueSquare,this);
WScript.Echo(y);

x = [10,20,30,40,50];
y = x.map(returnIndexSquare, this);
WScript.Echo(y);

x = [10,20,30,40,50];
y = x.map(returnRandom, this);
WScript.Echo(y);

x = {0: "abc", 1: "def", 2: "xyz"}
x.length = 3;

y = Array.prototype.map.call(x, returnValueSquare,this);
WScript.Echo(y);

y = Array.prototype.map.call(x, returnIndexSquare,this);
WScript.Echo(y);

y = Array.prototype.map.call(x, returnRandom, this);
WScript.Echo(y);

x = [10,20,30,40,50];
x[8] = 10;
y = x.map(returnValueSquare, this);
WScript.Echo(y);
