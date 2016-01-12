//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Check new Function with various inputs. Handles 2 parameters and a function body
function CheckNewFunction2(id, arg1, arg2, fnbody, expectsSuccess)
{
    try {
        var f = new Function(arg1, arg2, fnbody);
        if (expectsSuccess)
            WScript.Echo("PASS: " + id + ": new Function succeeded as expected");
        else
            WScript.Echo("FAIL: " + id + ": new Function should have failed, but did not");
    }
    catch (e)
    {
        if (expectsSuccess)
            WScript.Echo("FAIL: " + id + ": new Function should have succeeded, but did not. " + e);
        else
            WScript.Echo("PASS: " + id + ": new Function failed as expected. " + e);
    }
}

// Check new Function with various inputs. Handles 1 parameter and a function body
function CheckNewFunction1(id, arg1, fnbody, expectsSuccess)
{
    try {
        var f = new Function(arg1, fnbody);
        if (expectsSuccess)
            WScript.Echo("PASS: " + id + ": new Function succeeded as expected");
        else
            WScript.Echo("FAIL: " + id + ": new Function should have failed, but did not");
    }
    catch (e)
    {
        if (expectsSuccess)
            WScript.Echo("FAIL: " + id + ": new Function should have succeeded, but did not. " + e );
        else
            WScript.Echo("PASS: " + id + ": new Function failed as expected. " + e);
    }
}

// Check new Function with various inputs. Handles 0 parameters and a function body
function CheckNewFunction0(id, fnbody, expectsSuccess)
{
    try {
        var f = new Function(fnbody);
        if (expectsSuccess)
            WScript.Echo("PASS: " + id + ": new Function succeeded as expected");
        else
            WScript.Echo("FAIL: " + id + ": new Function should have failed, but did not");
    }
    catch (e)
    {
        if (expectsSuccess)
            WScript.Echo("FAIL: " + id + ": new Function should have succeeded, but did not. " + e);
        else
            WScript.Echo("PASS: " + id + ": new Function failed as expected. " + e);
    }
}

//
// Must pass in all modes
//
CheckNewFunction2(1, "a", "b", "return a+b", true);
// Repeat the same function to check if evalmap caching kicks in
CheckNewFunction2(1, "a", "b", "return a+b", true);
CheckNewFunction2(2, "a  ", "b  ", "  return a+b  ", true);
CheckNewFunction1(3, "a", "return a", true);
CheckNewFunction1(4, "a ", "return a ", true);
CheckNewFunction0(5, "return 23", true);
CheckNewFunction0(6, "return 23 ", true);
// It is allowed to have more than one formal listed in one of the strings
CheckNewFunction2(7, "a, b", "c", " return a+b+c", true);
CheckNewFunction1(8, "a,b", " return a+b", true);
CheckNewFunction1(9, "", " return 44", true);
// Check that we handle labels in the function body
CheckNewFunction2(10, "a", "b", "var c=a+b; loopbackhere: for(i=0; i<a; i++) { c+=i; if (i>10) break loopbackhere; }", true);

//
// Passes in pre-ES5, must fail in ES5
//
CheckNewFunction2(100, "a", "b", " return a+b } { var c=a+b; ", false);
// Repeat the same function to exercise evalmap functionality
CheckNewFunction2(100, "a", "b", " return a+b } { var c=a+b; ", false);
CheckNewFunction2(101, "a", "b", " return a+b } function foo() { return 1+2; ", false);
CheckNewFunction2(102, "a,b) { return a; } function foo(c ", "d", " return c+d", false);
CheckNewFunction0(103, "return 23; } function foo() { return 44; ", false);
CheckNewFunction1(104, "){}; function bug(){}\0", "", false);
CheckNewFunction1(105, 'a', 'return a}; {', false);

//
// Must fail in all modes
//
CheckNewFunction2(200, "", "", " return 1+2", false);
// Repeat the same function to exercise evalmap functionality
CheckNewFunction2(200, "", "", " return 1+2", false);
CheckNewFunction2(201, "", "a", " return a", false);
CheckNewFunction2(202, "a.b", "c.d", " return 42", false);
CheckNewFunction1(203, "23", " return 42", false);

// Valid syntax in ES5, should pass in ES5 mode
// Checks that the syntax validator knows about keywords 'get' and 'set'
CheckNewFunction0(300, "return {get x() {}}", true);
CheckNewFunction0(301, "({get x() {}})", true);
CheckNewFunction0(302, "return {set x(a) {}}", true);
CheckNewFunction0(303, "({set x(a) {}})", true);

// Valid syntax in ES5 and before, should pass
// Checks that the syntax validator knows about keywords 'eval' and 'arguments'
CheckNewFunction1(304, "a", "eval('a+1')", true);
CheckNewFunction1(305, "a", "return arguments[0]+2", true);
CheckNewFunction0(306, "({a:1, b:2})", true);
