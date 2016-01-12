//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function blah(){}
function bar(x)
{
    if (!x)
    throw 1;
    blah();

    return "Passed";
}

function test(x)
{
    WScript.Echo(bar(x));
}

try {
    test(0);
}catch(e)
{}
test(1);
