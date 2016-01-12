//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function returnTrue(x,y,z)
{
    WScript.Echo("value:"+ x + " index:" + y + " Object:" + z);
    return true;
}

function returnFalse(x,y,z)
{
    WScript.Echo("value:"+ x + " index:" + y + " Object:" + z);
    return false;
}

function returnRandom(x,y,z)
{
    WScript.Echo("value:"+ x + " index:" + y + " Object:" + z);
    return y!=1;
}

Array.prototype[6] = 20;

var x = [1,2,3,4,5];
var y = x.some(returnTrue,this);
WScript.Echo(y);

x = [10,20,30,40,50];
y = x.some(returnFalse, this);
WScript.Echo(y);

x = [10,20,30,40,50];
y = x.some(returnRandom, this);
WScript.Echo(y);

x = {0: "abc", 1: "def", 2: "xyz"}
x.length = 3;

y = Array.prototype.some.call(x, returnTrue,this);
WScript.Echo(y);

y = Array.prototype.some.call(x, returnFalse,this);
WScript.Echo(y);

y = Array.prototype.some.call(x, returnRandom, this);
WScript.Echo(y);

x = [10,20,30,40,50];
x[8] = 10;
y = x.some(returnTrue, this);
WScript.Echo(y);
