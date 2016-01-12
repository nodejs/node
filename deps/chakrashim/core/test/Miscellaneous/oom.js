//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var a = new Array(2000000);
var i = 0;
try
{
   while (true)
   {
       a[a.length] = new Object();
   }
}
catch (e)
{
}

for (var i = 0; i < 10; i++)
{
    WScript.Echo(i);
    CollectGarbage();
}
