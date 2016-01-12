//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Test initialization of let/const outside loop that we will try and box if loop body bails out.
function f1()
{
    {
        while (Math ^= [1])
        {
            Math;
        }
        let x = 0;
        {
            function foo() { WScript.Echo('foo') }
        }
        WScript.Echo(x);
    }
    let y = 1;
    WScript.Echo(y);
    foo();
}

f1();
f1();

WScript.Echo('pass');

