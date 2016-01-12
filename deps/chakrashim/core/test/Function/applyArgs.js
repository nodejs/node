//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function test1() 
{
    [].push.apply(this, arguments);
    write("passed");
}

test1();

function test2() 
{
    ({}).toString.apply(this, arguments);
    write("passed");
}

test2();

var count3 = 0;
function test3() 
{
    var args = arguments;
    function test3_inner() {
        (count3 == 1 ? args : arguments).callee.apply(this, arguments);
    }
    
    if (++count3 == 1)
    {
        return test3_inner();
    }
    
    write("passed");
}

test3();

function test4()
{
    return function() {
    try {
        throw 'zap';
    } catch(ex) {
        WScript.Echo(ex);
        var f = arguments[0]; 
    }
    f.apply(this, arguments);
    }
}
test4()(function(){ WScript.Echo('mama'); });
