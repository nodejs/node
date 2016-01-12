//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Lossless conversion to int32 for compensation, needs to bail out even with aggressive int spec disabled. Incr_A is not
// type-specialized on loop second pass, because 'a' is live on the back-edge and becomes a NumberValue on merge, in turn
// because '++a' may overflow a 32-bit integer during the loop. The lossless conversion to int32 therefore must include bailout,
// to account for the overflow. Ideally, we should not do the conversion to int32 on the back-edge, and instead make the var sym
// live in the landing pad (or loop header if it's only live inside the loop).
function test0(n) {
    var k = 0x3fffffff;
    k <<= 1;
    if(n === 1)
        --k;
    var a = k;
    while(n-- !== 0)
        ++a;
    return a;
}
WScript.Echo("test0: " + test0(2));

// Array element type specialization bug (uses lossy int32 index)
function test1() {
    var a = [1];
    a.foo = 2;
    function test1a(i) {
        var j = i & 1;
        return a[i] + a[j];
    }
    return test1a("foo");
}
WScript.Echo("test1: " + test1());

// Loop prepass, upon type specialization, should keep the destination value consistent with that sym's liveness. In this case,
// 'i' is not int-specialized in the loop prepass when aggressive int type specialization is disabled, so 'i' should be given
// a number or unknown value.
function test2() {
    var i = 0;
    do {
        ++i;
    } while(i < 1);
    return i;
}
WScript.Echo("test2: " + test2());

// - Lossy conversion of 'a' to int32 due to 'a | 0' should include a bailout on implicit calls since it may have a side effect
// - The FromVar with bailout should not be dead-store-removed due to the possibility of side effects
// - When lossy int type specialization is disabled, the resulting value of '~a' should still be an int range value so that the
//   '+' can still be int-specialized
// - Errors during ToPrimitive are handled appropriately by throwing after bailing out
function test3a(a) {
    a | 0;
    return ~a + 1;
}
WScript.Echo("test3a: " + test3a(-2));
function test3b(a) {
    a | 0;
    return ~a + 1;
}
WScript.Echo(
    "test3b: " +
    test3b(
        {
            valueOf:
                function () {
                    WScript.Echo("test3b: valueOf");
                    return -2;
                }
        }));
function test3c(a) {
    a | 0;
    return a | 0;
}
WScript.Echo("test3c: " + safeCall(test3c, { valueOf: null, toString: null }));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

function safeCall(f) {
    var args = [];
    for(var a = 1; a < arguments.length; ++a)
        args.push(arguments[a]);
    try {
        return f.apply(this, args);
    } catch(ex) {
        return ex.name;
    }
}
