//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// NOTE: because vdates are timezone specific, this test will might only work in PST
// not sure about DST.  If you see a failure, that would be my first suspicion

var date = new Date(0);

var vdate = date.getVarDate();

// Test string concat both ways:
writeLine("Attempt string concat (string + vdate) and echo.");
var appendMe = "test concat: " + vdate;
writeLine(appendMe);

writeLine("Attempt string concat  (vdate + string) and echo.");
var appendMe = vdate + ": test concat2";
writeLine(appendMe);

// Test typeof
writeLine(typeof(date));
writeLine(typeof(vdate));
writeLine(typeof(new Object(vdate)));
writeLine(typeof(Object(vdate)));

// Test .toString (should fail)
try {
    var myVar = vdate.toString();
    writeLine("FAIL: we should have errored on .toString();");
} catch (e)
{
    writeLine("SUCCESS: vdate.toString() failed with error #" + e.number);
}

// Test assigning to a member
try {
    vdate.aMember = 3;
    writeLine("FAIL: we should have errored on vdate.aMember = 3;");
} catch (e)
{
    writeLine("SUCCESS: vdate.aMember = 3 failed with error #" + e.number);
}

// Test assigning to a member with []
try {
    vdate["aMember"] = 3;
    writeLine("FAIL: we should have errored on vdate[\"aMember\"] = 3;");
} catch (e)
{
    writeLine("SUCCESS: vdate[\"aMember\"] = 3 failed with error #" + e.number);
}

// Test accessing a member
try {
    var shouldNotWork = vdate.aMember;
} catch (e)
{
    writeLine("SUCCESS: var shouldNotWork = date.aMember failed with error #" + e.number);
}

// Try some more unusual or invalid uses of VarDate
writeLine("");
writeLine("Unusual cases:");
vdate = new Date(1234567890123).getVarDate();
safeCall(function() { writeLine(vdate ? true : false); });
safeCall(function() { writeLine([1, 2].indexOf(2, vdate)); }); // valid only in version 3
safeCall(function() { writeLine(parseInt("1", vdate)); });
safeCall(function() { writeLine([1, vdate, 2].toLocaleString()); });

// Try some random dates to make sure we match the old engine
writeLine("");
writeLine("Pseudorandom cases:");
for (var i = 0; i < 1000; i++)
{
    var testDate = new Date(i*10373);
    var testVDate = testDate.getVarDate();
    writeLine("VT_DATE: '" + testVDate + "'");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers

function writeLine(str)
{
    WScript.Echo("" + str);
}

function safeCall(func)
{
    try
    {
        return func();
    }
    catch (ex)
    {
        writeLine(ex.name + " (" + ex.number + "): " + ex.message);
    }
}
