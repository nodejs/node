//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo()
{
    var s = "concat "+" string"+" test";
    for(var i=0;i<5;i++)
    {
        s = s+"dst same"+" as source";
    }
    s.charCodeAt(0);
}
foo();
foo();
foo();
WScript.Echo("Passed");
