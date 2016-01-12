//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var shouldReturn = true;
function func3(argMath8){
    try
    {
        var __loopvar6 = 5;
        argMath8++;
        func2.call(protoObj0 , 1);
    }
    catch(ex)
    {
        WScript.Echo(ex.message);
        if(shouldReturn)
        {
            return 1;
        }
    }
    ui32.length;
}

try
{
    func3(0);
    func3(0);
    shouldReturn = false;
    func3(1.1);
}
catch(ex){}

function v14()
{
    return 1;
}

function test0()
{
    var GiantPrintArray = [];
    var ary = new Array(10);
    try
    {
        GiantPrintArray.push(v14(ary[(1)],false));
        GiantPrintArray.push(v14(ary[(1)],1,1));
    }
    catch(ex)
    {
        WScript.Echo(ex.message);
    }
};

test0();
test0();
test0();
