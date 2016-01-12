//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// This bailout instruction should not be dead-store-removed after copy-prop
function test0() {
    var a = new Uint8Array(1);
    return a[-1] * 0 - 1 <= 0 ? false : true;
}
WScript.Echo("test0: " + test0());

// This bailout instruction should not be dead-store-removed after copy-prop
function test1() {
    var a = new Uint8Array(1);
    var b = a[-1] * 0;
    --b;
    return b <= 0 ? false : true;
}
WScript.Echo("test1: " + test1());

// This bailout instruction should not be dead-store-removed after copy-prop
function test2(a, b) {
    b &= 1;
    return (a * 0 + b) * 0 < 0 ? false : true;
}
for(var i = 0; i < 1000; ++i)
    test2(0, 0);
WScript.Echo("test2: " + test2({ valueOf: function() { WScript.Echo("test2: valueOf a"); } }, 0));

// This lossy conversion of 'a' to int32 should not be dead-store-removed since it has (or may have) side effects. Also, if the
// 'while' loop condition is not const-folded, due to 'a |= 0', the conversion would be changed into a lossless conversion of
// 'a' to int32. That conversion should also not be dead-store-removed even though const-prop would cause it to be a dead store,
// since it has (or may have) side effects.
function test3(a, b) {
    while((a & 1) * 0 + 1 == b * 0)
        a |= 0;
    return a * 0 - 1 <= 0 ? false : true;
}
for(var i = 0; i < 1000; ++i)
    test3(0, 0);
WScript.Echo("test3: " + test3({ valueOf: function() { WScript.Echo("test3: valueOf a"); } }, 0));

// - The value of 'm' becomes NumberAndLikelyIntValue on merge after the first loop
// - The assign to 'm' after that causes only the var version of 'm' to be live
// - '1 > m' in the second loop causes 'm' to be lossless int32-specialized and invariant-hoisted to the landing pad
// - The lossy conversion of 'm' in 'i |= m' should be removed and instead use the already hoisted lossless conversion
function test4() {
    var m = 1;
    for(var i = 0; i < 1; ++i)
        --m;
    1.1 ? 1 : m = g++;
    for(var i = 0; i < 1; ++i) {
        1.1 ? 1 : 1 > m ? 1 : 1;
        if(1)
            i |= m;
    }
    return m;
}
WScript.Echo("test4: " + test4());

// - Say 'a' gets value number 1 at the beginning
// - With aggressive int type spec, 'a = b' will give a new value number to 'a' in the loop prepass, say 2
// - Upon merging after the 'if', 'a' should be given a new value number, say 3. A bug occurs if it's given value number 1.
// - Upon merging on the loop back-edge, the entry value of 'a' must be different from the back-edge value of 'a' to signify
//   that its value changed inside the loop. If 'a' was given value number 1 upon merging after the 'if', it would appear as
//   though 'a' is invariant through the loop. If 'a' was given value number 3, since this is a loop back-edge, 'a' would be
//   given a NumberAndLikelyIntValue.
// - In 'return a', 'a' should not have an int constant value, and 'a' should not be constant-propagated to 0. If it was seen as
//   invariant through the loop, 0 would get constant-propagated here and the result would be incorrect. 'a' should have a
//   NumberAndLikelyIntValue at this point.
function test5(c) {
    var a = 0;
    for(var b = 0; b < c; ++b)
        if(c === 2)
            a = b;
    return a;
}
WScript.Echo("test5: " + test5(2));

// - In the loop prepass:
//     - At 'a = b', 'b' has an int value, so 'a' also has an int value
//     - At 'b = c', 'c' has an int value, so 'b' also has an int value
//     - At 'c += 1.1', 'c' now has a float value
// - After merge, 'a' and 'b' are live as ints on entry and have NumberAndLikelyInt values
// - In the optimization pass:
//     - At 'a = b', 'a' has a NumberAndLikelyIntValue, allowing it to be int-specialized
//     - At 'b = c', 'b' now has a float value
//     - On the back-edge, since 'b' was live as a lossless int32 in the loop header, the float value of 'b' needs to be
//       losslessly converted to an int
// - This is an almost guaranteed bailout case, and since a var version of 'b' is not available and float-to-int with bailout is
//   not supported, the compiler needs to bail out, disable aggressive int type specialization, and rejit the code gen work item
function test6() {
    var a, b = 0, c = 0;
    for(var i = 0; i < 3; ++i) {
        a = b;
        b = c;
        c += 1.1;
    }
    return a;
}
WScript.Echo("test6: " + test6());

// - Same as above, but requires one extra pass to flush out all dependencies
function test7() {
    var a, b = 0, c = 0, d = 0;
    for(var i = 0; i < 4; ++i) {
        a = b;
        b = c;
        c = d;
        d += 1.1;
    }
    return a;
}
WScript.Echo("test7: " + test7());

// - Profile data is taken only going through the 'else' block, so 'o.p' is flagged as likely int
// - When jitting with profile data, the two 'o.p' accesses share inline caches, so the reference in the 'if' block also says
//   it's likely int, even though it's definitely not (a string is assigned to 'o.p' just before in the 'if' block)
// - The source of the add in the 'if' block is copy-propped with the sym holding the string, and the load-field is changed into
//   a direct load. The destination of that is a single-def sym, so the sym is marked as definitely not int.
// - Even though the profile data says that the value is likely int, it should not override the "definitely not int" flag on the
//   sym since it will guarantee bailout.
// - Note: The guaranteed bailout problem still exists with 'o' not being a slot variable since that causes 'o.p' to be hoisted
//   into a stack sym that is not single-def, so we lose the information that it's definitely not an int in the 'if' block
function test8(a) {
    var o = { p: 0 };
    for(var i = 0; i < 1; ++i) {
        if(a) {
            o.p = "";
            ++o.p;
        } else
            ++o.p;
    }
    return o.p;

    function test8a() { o; }
}
WScript.Echo("test8: " + test8(false));

// - At 'u++', there is a Conv_Num of the original value before the increment
// - In the loop prepass, since 'u' starts off as an int32, the source of the Conv_Num has an int constant value, and Conv_Num
//   is int-specialized
// - In the loop prepass, when aggressive int type specialization is off, the destination value of Conv_Num must not be
//   transferred from the source, because 'u' is changing in the loop. Since we don't yet know that 'u' is changing (the add
//   comes after Conv_Num), the destination must be given a new value. Otherwise, if the destination sym is live as an int on
//   entry into the loop, it will have to do a lossless conversion to int on the loop back-edge, and that's not allowed when
//   aggressive int type specialization if off.
function test9() {
    var f, b, u;
    for(var r = [[0]], o = 0; o < r.length; ++o) {
        r[0].length = (0x3fffffff << 1) + 3;
        for(f = r[o], b = f.length, u = 0x3fffffff << 1; u < b; u++)
            b !== 0 && r.push(0);
    }
}
WScript.Echo("test9: " + test9());

// - The Sub is not int-specialized because it's not worth specializing it
// - Since the result of the Sub is used in a bitwise operation, int overflow is ignored on the sub
// - However, since the Sub is not specialized, the result of the Sub should not be given a definitely-int value since its srcs
//   will not be converted to int with bailout checks to ensure that they're ints
function test10(a) {
    return (610611150 * 1322123869 - a) | 0;
};
WScript.Echo("test10: " + test10(0));

// - 'a = (1 - a) * -1' is rewritten by the lowerer as the following, to make the destination and first source the same:
//     a = 1 - a
//     a = a * -1
// - When 'a * -1' bails out due to producing -0, the value of 'b' must be restored. It must not be restored from 'a' because it
//   changed before the bailout (at 'a = 1 - a'). It's ok that it changed because it will be overwritten anyway. Instead, the
//   sym corresponding to 'b', which must still be live due to the use of 'b' later, should be used to restore.
function test11() {
    var a = 0;
    for(var i = 0; i < 1; ++i)
        a = 1;
    var b = a;
    a = (1 - a) * -1;
    return b;
};
WScript.Echo("test11: " + test11());
