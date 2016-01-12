//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//Switches:   -bgJit- -maxInterpretCount:2

// This test verifies that we mark all types (not just the type of the object being modified) as potentially having been
// updated whenever we a) store a property using a live cache or b) add a property via an object type spec'ed instruction.
function test0(condition) {
    var obj0 = {};
    obj0.prop0 = 1;

    var obj1 = {};
    obj1.prop0 = 1;

    if (condition) {
        obj1 = obj0;
    }

    (function (o, p) {
        var v = o.prop0;
        p.a = 11;
        p.b = 12;
        o.x = 21;
        o.y = 22;
    })(obj0, obj1);

    WScript.Echo("obj0 === obj1: " + (obj0 === obj1));
    WScript.Echo("obj0: { prop0: " + obj0.prop0 + ", a: " + obj0.a + ", b: " + obj0.b + ", x: " + obj0.x + ", y: " + obj0.y + " }");
    WScript.Echo("obj1: { prop0: " + obj1.prop0 + ", a: " + obj1.a + ", b: " + obj1.b + ", x: " + obj1.x + ", y: " + obj1.y + " }");
};

WScript.Echo("Test0:");
test0(false);
test0(false);

test0(true);

// This test is a slight variation of the same theme. In this case we get one of the object to modify not from an input argument,
// but from a closure slot. As a result we may not optimize its property add sequence, because the object syms are different in
// the backward pass (before copy prop changes them in the forward pass), and thus appear dead in the forward pass.
function test1(condition) {
    var obj0 = {};
    obj0.prop0 = 1;

    var obj1 = {};
    obj1.prop0 = 1;

    if (condition) {
        obj1 = obj0;
    }

    (function (o) {
        var v = o.prop0;
        obj1.a = 11;
        obj1.b = 12;
        o.x = 21;
        o.y = 22;
    })(obj0);

    WScript.Echo("obj0 === obj1: " + (obj0 === obj1));
    WScript.Echo("obj0: { prop0: " + obj0.prop0 + ", a: " + obj0.a + ", b: " + obj0.b + ", x: " + obj0.x + ", y: " + obj0.y + " }");
    WScript.Echo("obj1: { prop0: " + obj1.prop0 + ", a: " + obj1.a + ", b: " + obj1.b + ", x: " + obj1.x + ", y: " + obj1.y + " }");
};

WScript.Echo("Test1:");
test1(false);
test1(false);

test1(true);
