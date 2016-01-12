//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//
// Test for-in enumerating properties on prototype of primitives
//
var echo = WScript.Echo;

function guard(f) {
    try {
        f();
    } catch (e) {
        echo(e);
    }
}

Object.prototype.object_value = "value on Object.prototype";
Object.defineProperty(
    Object.prototype, "object_getter", {
        get: function () { return "getter on Object.prototype: " + typeof(this) + " " + this; },
        enumerable: true,
        configurable: true
    });

Number.prototype.number_proto = "Value on Number.prototype";
Boolean.prototype.boolean_proto = "Value on Boolean.prototype";
String.prototype.string_proto = "Value on String.prototype";

// Test on special values and primitives
var tests = [
    null,
    undefined,
    Number.NaN,
    0.4,
    -0,
    0,
    1,
    "",
    true,
    false,

    // Special values around Int32/Int31 boundary
    0x80000000,
    0x7FFFFFFF,
    0x40000000,
    0x3FFFFFFF,
    -0x3FFFFFFF,
    -0x40000000,
    -0x40000001,
    -0x80000000,
    -0x80000001,
];

tests.forEach(function (a) {
    echo("---- Test:", a, "----");

    guard(function () {
        echo(a.object_value); // Get from prototype
    });
    guard(function () {
        echo(a.object_getter); // Getter on prototype
    });

    echo();
    for (var p in a) { // Enumerate properties should walk prototype
        echo(" ", p);
    }

    echo();
});
