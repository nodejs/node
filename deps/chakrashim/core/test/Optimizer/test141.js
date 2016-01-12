//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0(o) {
    o.p0 = o.p0 ? o.p0.replace(/z/, "z") : test0a();
    o.p1 = o.p1 || "z";
    o.p2 = 1;
    return o;
}
function test0a() {
    try { } catch(ex) { }
};
test0({ p0: "a", p1: "b" });
test0({ p0: "a", p1: "b" });
test0({ p0: "a", p1: "b" });

WScript.Echo("pass");
