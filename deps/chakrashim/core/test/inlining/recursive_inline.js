//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var obj = {get x(){return 2;}};
function foo(y)
{
    if(y == 0) return;
    obj.x;
    foo(--y);
}
foo(4);
WScript.Echo("PASSED");
