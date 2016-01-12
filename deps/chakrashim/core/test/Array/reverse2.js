//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

for (var i=0;i<100; i += 2)
{
    Array.prototype[i] = (i*i) + 1000;
}

function Test()
{

    var args = arguments;
    var a = new Array();

    while (args.length > 1)
    {

        var s = Array.prototype.shift.call(args);
        var e = Array.prototype.shift.call(args);

        for (var i=s;i<e;i++)
        {
            a[i] = i;
        }

    }
    a.length = Array.prototype.shift.call(args);

    write(a);
    write(a.reverse());
    write(a.reverse());
}

Test(0,10,10);
Test(0,5, 7,15,15);
Test(0,5, 7,15, 21,24,30);
Test(0,5, 7,15, 21,24, 55, 59 , 65);
Test(0,5, 7,15, 21,24, 55, 59 , 78);
Test(0,1, 7,12, 15,17, 26, 27 , 27);

