//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

if (this.WScript) { WScript.LoadScriptFile("TrimStackTracePath.js"); }

function dump(output)
{
    if (WScript != undefined)
    {
        WScript.Echo(output);
    }
    else
    {
        alert(output);
    }
}

function dumpProperties(e)
{
    var output = "Properties of e:\n";
    var p;
    for (p in e)
    {
        output += "    " + p + "\n";
    }
    output += "\ne.number: " + e.number
          + "\ne.stack: " + TrimStackTracePath(e.stack);
    dump(output);
}

function ensureCanDeleteStackProperty(e)
{
    dump("\n\nDeleting e.stack ...\n");
    try
    {
        delete e.stack
    }
    catch (e2)
    {
        dump("Exception when deleting stack property: " + e2);
    }
    dump("e.stack after delete:\n" + TrimStackTracePath(e.stack));
}

function ensureCanUpdateStackProperty(e)
{
    dump("\n\nUpdating e.stack ...\n");
    try
    {
        e.stack = "foo";
    }
    catch (e2)
    {
        dump("Exception when updating stack property: " + e2);
    }
    dump("e.stack after update:\n" + TrimStackTracePath(e.stack));
}

function throwException()
{
    try
    {
        BadType.someProperty = 0;
    }
    catch(e)
    {
        dumpProperties(e);
        ensureCanDeleteStackProperty(e);
    }
    try
    {
        BadType.someProperty = 0;
    }
    catch(e)
    {
        dumpProperties(e);
        ensureCanUpdateStackProperty(e);
    }
}

function bar()
{
    throwException();
}

function foo()
{
    bar();
}

foo();


function preventExt() {
    try {
        dump("");
        dump("Object.preventExtensions test:");
        var x = Error();
        dump(x.hasOwnProperty("stack"));
        Object.preventExtensions(x);
        dump(x.hasOwnProperty("stack"));
        throw x;
    } catch (e) {
        dump(x.hasOwnProperty("stack"));
        dump(e.hasOwnProperty("stack"));
    }
}

preventExt();

