//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function returnSum(w,x,y,z)
{
    WScript.Echo("accumulator:" + w + " value:"+ x + " index:" + y + " Object:" + z);
    return w + x;
}

function returnSquare(w,x,y,z)
{
    WScript.Echo("accumulator:" + w + " value:"+ x + " index:" + y + " Object:" + z);
    return w + x * x;
}

function returnStringSquare(w,x,y,z)
{
    WScript.Echo("accumulator:" + w + " value:"+ x + " index:" + y + " Object:" + z);
    return w + x + x;
}

function returnRandom(w,x,y,z)
{
    WScript.Echo("accumulator:" + w + " value:"+ x + " index:" + y + " Object:" + z);
    return w + x + y;
}

Array.prototype[6] = 20;

var x = [1,2,3,4,5];
var y = x.reduceRight(returnSum,0);
WScript.Echo(y);

x = [10,20,30,40,50];
y = x.reduceRight(returnSquare, 0);
WScript.Echo(y);

x = [10,20,30,40,50];
y = x.reduceRight(returnRandom);
WScript.Echo(y);

x = {0: "abc", 1: "def", 2: "xyz"}
x.length = 3;

y = Array.prototype.reduceRight.call(x, returnSum, "" );
WScript.Echo(y);

y = Array.prototype.reduceRight.call(x, returnStringSquare, "");
WScript.Echo(y);

y = Array.prototype.reduceRight.call(x, returnRandom, "");
WScript.Echo(y);

x = [10,20,30,40,50];
x[8] = 10;
y = x.reduceRight(returnSum, 30);
WScript.Echo(y);
