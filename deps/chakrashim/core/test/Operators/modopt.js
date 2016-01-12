//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

// Bailout cases
function modByNeg(x)
{
    return 32 % x;
}
function modByNonPowerOf2(x)
{
    return 32 % x;
}

function modOfNeg(a, x)
{
   a = a | 0; // forces typespec
   return a % x;
}

function runTest()
{
    write(modByNeg(16)); // by power of 2
    write(modByNeg(-3)) // cause bailout
    
    write(modByNonPowerOf2(16*16)); // by power of 2
    write(modByNonPowerOf2(23)) // cause bailout
    
    write(modOfNeg(100, 32));
    write(modOfNeg(-12, 32)); // cause bailout
    
    for(var i=0; i < 500; i++)
    {
        modByNeg(-3); // cause rejit
    }
}

runTest();
