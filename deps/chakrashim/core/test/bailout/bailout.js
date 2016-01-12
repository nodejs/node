//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var a =0;
function func()
{
    // injected bailout point #1 
    return 3;
}


// injected bailout point #2
for (var i = 0; i < 10; i++)
{
    a += func();
}

WScript.Echo(a);
