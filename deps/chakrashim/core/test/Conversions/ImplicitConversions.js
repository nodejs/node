//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var toStrings;
var valueOfs;
var toStringCalled;
var valueOfCalled;

toStrings =
[
    {},
    function ()
    {
        toStringCalled = true;
        return {};
    },
    function ()
    {
        toStringCalled = true;
        return undefined;
    },
    function ()
    {
        toStringCalled = true;
        return "hi";
    }
];

valueOfs =
[
    {},
    function ()
    {
        valueOfCalled = true;
        return {};
    },
    function ()
    {
        valueOfCalled = true;
        return undefined;
    },
    function ()
    {
        valueOfCalled = true;
        return "hi";
    },
    function ()
    {
        valueOfCalled = true;
        return "1/1/1970 1:00 am";
    },
    function ()
    {
        valueOfCalled = true;
        return "84";
    },
    function ()
    {
        valueOfCalled = true;
        return 37;
    }
];

for (var ts in toStrings)
{
    for (var vo in valueOfs)
    {
        toStringCalled = false;
        valueOfCalled = false;

        var obj = { toString: toStrings[ts], valueOf: valueOfs[vo] };

        WScript.Echo("=== Implicit toString ===");
        try
        {
            WScript.Echo("" + obj);
        }
        catch (ex)
        {
            WScript.Echo("Got error:");
            WScript.Echo("    name:     " + ex.name);
            WScript.Echo("    message:  " + ex.message);
        }
        WScript.Echo("toString called:  " + (toStringCalled ? "yes" : "no"));
        WScript.Echo("valueOf called:   " + (valueOfCalled ? "yes" : "no"));

        WScript.Echo("=== Implicit valueOf ===");
        try
        {
            WScript.Echo(1 * obj);
        }
        catch (ex)
        {
            WScript.Echo("Got error:");
            WScript.Echo("    name:     " + ex.name);
            WScript.Echo("    message:  " + ex.message);
        }
        WScript.Echo("toString called:  " + (toStringCalled ? "yes" : "no"));
        WScript.Echo("valueOf called:   " + (valueOfCalled ? "yes" : "no"));
    }
}
