//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

try
{
    var longString = "A";
    for (var i = 0; i < 31; i++)
        longString += longString;
    WScript.Echo(longString.toString());
}
catch (e)
{
    WScript.Echo(e.name);
    WScript.Echo(e.message);
    WScript.Echo(e.description);
    WScript.Echo(e.number);
}

