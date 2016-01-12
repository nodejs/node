//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// -force:fieldhoist -prejit
(function(){
    var obj6 = 1;
    LABEL0:
    LABEL1:
    for (var __loopvar0 = 0; obj6.a < (1) && __loopvar0 < 3; obj6.a++ + __loopvar0++) {
    }
})();

// - 'o1.a = 1' kills the hoisted o2.a as being live
// - As part of the load in '++o2.a' it reloads field into the hoisted stack sym. This load is int-specialized, making the
//   hosited stack sym only live as an int.
// - As part of the store in '++o2.a' it loads the value to store into the hoisted stack sym and then stores the hoisted stack
//   sym value into the field. As part of this load, since only the int version of the hoisted stack sym was live, liveness
//   needs to be updated to say that only the var version of the hoisted stack sym is now live.
(function() {
    var o1 = { a: 0 };
    var o2 = o1;
    for (var i = 0; i < 1; ++i)
        for (; o2.a < 1; ++o2.a)
            o1.a = 1;
})();

function test0() {
    function test0a(o) {
        o.p = "1";
    }

    // - 'o.p' is live around the outer loop, so it's hoisted
    // - At 'o.p &= 1' the hoisted stack sym is only live as an int32
    // - In the prepass of the inner loop, at 'o.q = test0a(o)', 'o.p' is killed. Ideally, here the hoisted int32 stack sym
    //   should also be killed, but it's not since it requires more tracking and computation.
    // - In the optimization pass of the inner loop, the use at 'o.p | 0' requires a reload since 'o.p' is not live. At this
    //   point, the field should not have been live on entry into the loop either (after merge with prepass), so it should also
    //   kill the hoisted int32 stack sym.
    // - This prevents compensation to be required on the back-edge of the inner loop in the optimization pass, which would in
    //   this case, cause an unnecessary (and permanent) bail-out even when aggressive int type spec is off (hence the assert).
    function test0b(o) {
        var sum = 0;
        for (var i = 0; i < 10; ++i) {
            sum += o.p &= 1;
            for (var j = 0; j < 10; ++j) {
                sum += o.p | 0;
                o.q = test0a(o);
            }
            sum += o.p | 0;
        }

        return sum;
    }

    return test0b({ p: 1, q: 1 });
}
WScript.Echo("test0: " + test0());

// - 'a' is a slot variable since it's used in a child function
// - In 'b = a = c ? 1 : 2', the sym for 'b' is only available as an int32 and 'b' is the sym store for that value
// - At 'a = a', the field (slot) load is hoisted. It is found that the field's value number in the landing pad is the same as
//   the value's sym store's value number. So, instead of loading the field in the landing pad, 'b' is copied instead.
// - Since 'b' is only available as an int32, it needs to be converted to var.
function test1() {
    var c = 1;
    function test1a() {
        var a;
        for (var i = 0; i < 1; ++i) {
            var b = a = c ? 1 : 2;
            for (var j = 0; j < 1; ++j)
                a = a;
        }
        function test1aa() { a; }
        return a;
    }
    return test1a();
}
WScript.Echo("test1: " + test1());

// - 'a' is considered a field since it's used by a child function
// - 'a' is hoisted outside the outer loop into a stack sym
// - 'a |= 0' makes the hoisted stack sym for 'a' available as var and lossless int32
// - 'test2a()' should kill the hoisted stack syms for 'a' since that function could have changed 'a'
// - In this case, by the time of 'c = a' in the inner loop's second prepass, there are hoistable fields. Regardless of whether
//   the loop has hoistable fields, the call should kill the hoisted stack syms for 'a' (the kill actually happens upon reload
//   at 'c = a').
function test2() {
    var a, b, c;
    for (var i = 0; i < 1; ++i) {
        a |= 0;
        b = 0;
        for (var j = 0; j < 1; ++j) {
            0 ? b : 0;
            null ? b = test2a() : a;
            c = a;
        }
    }
    function test2a() { a, b; }
    return c;
}
WScript.Echo("test2: " + test2());

// - 'b' gets hoisted outside the most outer loop
// - 'b = -158506649.9 >> 1' makes the hoisted stack sym for 'b' available as an int32
// - 'test3a()' kills 'b'. However, hoisted stack syms are not killed when fields are killed, rather, they need to be killed on
//   the next reload.
// - Because 'b' is not live coming into the most inner loop, 'b' is hoistable in the prepass
// - The use of 'b' in 'b >>= b' and the lack of kills in the most inner loop cause 'b' to be hoisted outside that loop
// - Since 'b' was already hoisted, the hoisted stack sym is reused
// - Note that the hoisted stack sym from the most outer loop is available an an int32 in the most inner loop's landing pad
// - Hoisting 'b' into the most inner loop's landing pad counts as a reload, so it needs to kill specialized versions of the
//   hoisted stack sym in the landing pad, loop header, and in the on-entry info to prevent forcing compensation
function test3() {
    var a = 0;
    var b = 0;
    for (var i = 0; b !== 1 && i < 1; ++i) {
        b = -158506649.9 >> 1;
        for (var j = 0; j < 8; ++j) {
            test3a();
            ++a;
            for (var k = 0; (b >>= b) && k < 1; ++k)
                a >>>= 1;
        }
    }
    return a;

    function test3a() {
        a;
        ++b;
    }
}
WScript.Echo("test3: " + test3());

// - 'a' is a slot variable since it's used by a child function, and is hoisted into a stack sym as a field
// - 'a &= 1' makes the hoisted stack sym available as an int
// - In 'a = 1', the constant is loaded into a stack sym ('s1 = 1') somewhere before, so the inner loop's last prepass sees
//   'a = s1'. Since 's1 = 1' would have already been int-specialized, 's1' would be available as an int and the hoisted stack
//   sym of 'a' will also be made to be available as an int.
// - In the optimization pass, due to constant propagation, 'a = 1' appears as 'a.var = 0x3'. Since the loop prepass made the
//   hoisted stack sym for 'a' available as an int, the optimization pass should do so as well (prepass must be equally or less
//   aggressive than the optimziation pass).
// - By the end, the int version of the hoisted stack sym of 'a' should be live through the inner loop and compensation should
//   not be necessary
function test4() {
    var a = 0;
    for (var i = 0; i < 1; ++i) {
        a &= 1;
        for (var j = 0; j < 1; ++j)
            a = 1;
    }
    return a;

    function test4a() { a; }
}
WScript.Echo("test4: " + test4());

// Same as above but with a field instead of a slot variable
function test5() {
    var o = { a: 0 };
    for (var i = 0; i < 1; ++i) {
        o.a &= 1;
        for (var j = 0; j < 1; ++j)
            o.a = 1;
    }
    return o.a;
}
WScript.Echo("test5: " + test5());
