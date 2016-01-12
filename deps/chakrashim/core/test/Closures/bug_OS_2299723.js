//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// let and const variables should exhibit redeclaration and assignment to const errors
// even when they are located in an ActivationObjectEx cached scope.
// Test them independently
//
function f0() {
    let x = 0;

    try {
        eval("var x = 5");
    } catch (e) {
        WScript.Echo("eval('var x = 5') threw '" + e.message + "'");
    }

    try {
        eval("x = 5");
    } catch (e) {
        WScript.Echo("unexpected error thrown: '" + e.message + "'");
    }

    WScript.Echo("x: " + x);
}

// Called-in-loop is no longer the heuristic we want to use to enable scope caching.
// Instead rely on -force:cachedscope and call the test function only once here.
f0();

function f1() {
    const y = 1;

    try {
        eval("var y = 5");
    } catch (e) {
        WScript.Echo("eval('var y = 5') threw '" + e.message + "'");
    }

    try {
        eval("y = 5");
    } catch (e) {
        WScript.Echo("eval('y = 5') threw '" + e.message + "'");
    }

    WScript.Echo("y: " + y);
}

// Called-in-loop is no longer the heuristic we want to use to enable scope caching.
// Instead rely on -force:cachedscope and call the test function only once here.
f1();
