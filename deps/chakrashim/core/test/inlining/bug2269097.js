//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function init(data, length)
{
    var i;
    for(i=0; i< length; i++)
    {
        data[i] = 0;
        data[i] = i + i;
        data[i] = i - i;
        data[i] = i * i;
        data[i] = i - i;
        data[i] = i + i;
        data[i] = i - i;
        data[i] = i * i;
    }

}
function fib(n)
{
    if(n <= 1)
        return 1;

    return fib(n-1) + fib(n-2);
}
function bar(data0, data1, data2, length)
{
    init(data0, length);
    init(data1, length);
    init(data2, length);
    init(data0, length);
    init(data1, length);
}

function foo()
{
    var data0 = new Array(100);
    var data1 = new Array(100);
    var data2 = new Array(100);
    bar(data0, data1, data2, 100);
    bar(data0, data1, data2, 100);
    bar(data0, data1, data2, 100);
    bar(data0, data1, data2, 100);
    bar(data0, data1, data2, 100);
    bar(data0, data1, data2, 100);
    bar(data0, data1, data2, 100);
    fib(10);
}

foo();
foo();

WScript.Echo("PASSED");
