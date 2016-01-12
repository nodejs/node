//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var obj = new Object();
obj.n = null;
obj.blah = 1;
obj[0] = null;
obj[1] = 1;

var n = null;
var blah = 1;

// Call integer global var
function CallErrorTest0()
{
    blah();
}

// new integer global var 
function CallErrorTest1()
{
    new blah();
}

// Call integer property
function CallErrorTest2()
{
    obj.blah();
}

// new integer property
function CallErrorTest3()
{
    new obj.blah();
}

// Call primitive null
function CallErrorTest4()
{
    null();
}

// New primitive null
function CallErrorTest5()
{
    new null();
}

// Call integer
function CallErrorTest6()
{
    1();
}

// new integer
function CallErrorTest7()
{
    new 1();
}

// Call parameter
function CallErrorTest8()
{
    function func(f)
    {
        f();
    }

    func(1);
}

// Call index - null
function CallErrorTest9()
{
    obj[0]();
}

// new index - null 
function CallErrorTest10()
{
    new obj[0]();
}

// Call index - number
function CallErrorTest11()
{
    obj[1]();
}

// new index - number
function CallErrorTest12()
{
    new obj[1]();
}

// Call index on null
function CallErrorTest13()
{
    n[1]();
}

// new index on null
function CallErrorTest14()
{
    new n[1]();
}

// Call property on null
function CallErrorTest15()
{
    n.prop();
}

// new property on null
function CallErrorTest16()
{
    new n.prop();
}

// Call null property 
function CallErrorTest17()
{
    obj.n();
}

// new null property 
function CallErrorTest18()
{
    new obj.n();
}

// Call null global var
function CallErrorTest17()
{
    n();
}

// new null global var
function CallErrorTest18()
{
    new n();
}

var i = 0;
while (this["CallErrorTest" + i] != undefined)
{
    safeCall(this["CallErrorTest" + i]);
    i++;
}

writeLine("");
writeLine("");

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The following tests invalid function calls on unresolvable vars, nonexisting and existing globals accessed using different
// methods, local vars, objects, arrays, and deleted properties. Test functions are generated for each case and evaluated.

var testCase_implicitGlobal = "implicit global";
var testCase_globalUsingWindow = "global using window";
var testCase_globalUsingThis = "global using this";
var testCase_local = "local";
var testCase_object = "object";
var testCase_arrayInitialized = "array initialized";
var testCase_arrayAssigned = "array assigned";
var testCases =
[
    testCase_implicitGlobal,
    testCase_globalUsingWindow,
    testCase_globalUsingThis,
    testCase_local,
    testCase_object,
    testCase_arrayInitialized,
    testCase_arrayAssigned
];

var testValue_uninitialized = null; // don't initialize before calling as a function
var testValue_undefined = "undefined";
var testValue_null = "null";
var testValue_number = "1";
var testValue_object = "{}";
var testValues =
[
    testValue_uninitialized,
    testValue_undefined,
    testValue_null,
    testValue_number,
    testValue_object
];

var globalIndex = 0;
function generateAndRunTests(testCase, doDelete)
{
    if (testCase === testCase_local && doDelete)
        return; // deleting is not supported for this test case

    writeLine("--- Test case: " + testCase + ", do delete: " + doDelete + " ---");
    writeLine("");

    for (var testValueIndex in testValues)
    {
        var testValue = testValues[testValueIndex];

        // A function that looks like the following is generated, and varies based on the test case and test value. The function
        // below is generated for the following parameters: {testCase_arrayAssigned, testValue_object, doDelete = true}
        //        safeCall(function ()
        //        {
        //            var a = [];
        //            a[0] = {};
        //            delete a[0];
        //            a[0]();
        //        });

        var globalIdentifier;
        switch (testCase)
        {
            case testCase_implicitGlobal:
                globalIdentifier = "g" + globalIndex++;
                break;
            case testCase_globalUsingWindow:
                globalIdentifier = "window.g" + globalIndex++;
                break;
            case testCase_globalUsingThis:
                globalIdentifier = "this.g" + globalIndex++;
                break;
        }

        var f = "safeCall(function(){";
        switch (testCase)
        {
            case testCase_implicitGlobal:
            case testCase_globalUsingWindow:
            case testCase_globalUsingThis:
                if (!testValue && doDelete)
                    continue; // no need to delete an uninitialized property
                if (testCase === testCase_globalUsingWindow)
                    writeLine("Only valid in IE:"); // the result of this test is only valid when run in IE since 'window' is undefined otherwise
                if (testCase === testCase_globalUsingThis && (!testValue || doDelete))
                    writeLine("INCORRECT in JC all versions:"); // BUG: these cases produce incorrect results in JC (all versions) but work in IE
                if (testValue)
                    f += globalIdentifier + "=" + testValue + ";";
                if (doDelete)
                    f += "delete " + globalIdentifier + ";";
                f += globalIdentifier + "();";
                break;

            case testCase_local:
                f += "var v";
                if (testValue)
                    f += "=" + testValue;
                f += ";v();";
                break;

            case testCase_object:
                if (!testValue && doDelete)
                    continue; // no need to delete an uninitialized property
                f += "var o={";
                if (testValue)
                    f += "p:" + testValue;
                f += "};"
                if (doDelete)
                    f += "delete o.p;";
                f += "o.p();";
                break;

            case testCase_arrayInitialized:
                if (!testValue && doDelete)
                    continue; // no need to delete an uninitialized property
                if (testValue === testValue_undefined && !doDelete)
                    writeLine("INCORRECT in compat modes:"); 
                f += "var a=[";
                if (testValue)
                    f += testValue;
                f += "];"
                if (doDelete)
                    f += "delete a[0];";
                f += "a[0]();";
                break;

            case testCase_arrayAssigned:
                if (!testValue && doDelete)
                    continue; // no need to delete an uninitialized property
                f += "var a=[];";
                if (testValue)
                    f += "a[0]=" + testValue + ";";
                if (doDelete)
                    f += "delete a[0];";
                f += "a[0]();";
                break;

            default:
                writeLine("Unknown test case '" + testCase + "'.");
                return;
        }
        f += "});";

        writeLine(f);
        eval(f);
        writeLine("");
    }

    writeLine("");
}

var booleans = [false, true];
for (var testCaseIndex in testCases)
{
    var testCase = testCases[testCaseIndex];
    for (var doDeleteIndex in booleans)
    {
        var doDelete = booleans[doDeleteIndex];
        generateAndRunTests(testCase, doDelete);
    }
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
