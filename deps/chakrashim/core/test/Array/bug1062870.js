//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var a = [];
a[4294967294] = 8;
try
{
    a.splice(4294967295,0,1); //length grows by 1
}
catch(e)
{
    WScript.Echo("PASS");
}