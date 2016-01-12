//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0(){
var s = "aabbcc";
var r = new RegExp(/b/);
r.lastIndex = 15;
s.split(r);
WScript.Echo(r.lastIndex !== 15 ? "Pass" : ("Fail actual value: " + r.lastIndex));
}
test0();
test0();
