//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var x = {a:0, b:10, c:20};
for (i in x)
{
    WScript.Echo(i + " = " + x[i]);
}
delete x.b;
for (i in x)
{
    WScript.Echo(i + " = " + x[i]);
}
x.b = 23;
for (i in x)
{
    WScript.Echo(i + " = " + x[i]);
}
delete x.b;
for (i in x)
{
    WScript.Echo(i + " = " + x[i]);
}
