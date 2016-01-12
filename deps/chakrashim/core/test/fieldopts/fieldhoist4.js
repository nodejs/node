//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function fieldhoist1()
{
    var object = {};

    var sum = 0;
    for (var i = 0; i < 3; i++)
    {
        sum += object.sum;      // hoisted field load
        Object.defineProperty(object, "sum", { get: function() { WScript.Echo("sum" ); }, configurable: true });
        sum += object.sum;      // implicit call bailout
    }
}

function fieldhoist2()
{
    var object = {};

    var sum = 0;
    for (var i = 0; i < 3; i++)
    {
        sum += object.sum;      // hoisted field load
        Object.defineProperty(object, "x", { get: function() { WScript.Echo("x"); }, configurable: true });  // kill all fields
        sum += object.sum;      // reload, no bailout
    }
}

function fieldhoist3(name)
{
    var object = { sum: 1};

    Object.defineProperty(object, name, { set: function(val) { WScript.Echo(val); }, configurable: true });
    var sum = 0;
    for (var i = 0; i < 3; i++)
    {
        sum += object.sum;      // hoisted field load
        object[name] = object.sum;       // kill all fields
        sum += object.sum;      // reload, no bailout
    }
}

function main()
{
    fieldhoist1();
    fieldhoist2();
    fieldhoist3("x");
}

main();
