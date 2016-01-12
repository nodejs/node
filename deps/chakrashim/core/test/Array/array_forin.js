//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function DumpObject(o)
{
    for (var i in o)
    {
        WScript.Echo(i + " = " + o[i]);
    }
}

var a = new Array(0xFFFFFFFF);
WScript.Echo(a.length);
a[0xFFFFFFFE] = 1;

DumpObject(a);

function Foo()
{
}

Foo.prototype[3] = 101;

var o = new Foo();
o[3] = 3;
o[4] = 4;

DumpObject(o);

