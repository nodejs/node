//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var obj = {get x(){return 2;}};
function a()
{
}
function b()
{
}

function foo(x, y)
{
    if(y == 0) return;
    x();
    foo(b, --y);
}
foo(a,4);
WScript.Echo("PASSED");
