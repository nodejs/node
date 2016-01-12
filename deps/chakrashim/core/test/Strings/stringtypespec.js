//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test(s1, s2, s3, s4, s5, s6, s7, s8) {
    if (s1 == s2) { WScript.Echo("fail"); } else { WScript.Echo("pass"); }
    if (s1 === s2) { WScript.Echo("fail"); } else { WScript.Echo("pass"); }
    if (s1 != s2) { WScript.Echo("pass"); } else { WScript.Echo("fail"); }
    if (s1 !== s2) { WScript.Echo("pass"); } else { WScript.Echo("fail"); }
    if (s1 == s3) { WScript.Echo("pass"); } else { WScript.Echo("fail"); }
    if (s1 === s3) { WScript.Echo("pass"); } else { WScript.Echo("fail"); }
    if (s1 != s3) { WScript.Echo("fail"); } else { WScript.Echo("pass"); }
    if (s1 !== s3) { WScript.Echo("fail"); } else { WScript.Echo("pass"); }
    if (s1.charAt(0) == s4) { WScript.Echo("pass"); } else { WScript.Echo("fail"); }
    if (s1.charAt(0) === s4) { WScript.Echo("pass"); } else { WScript.Echo("fail"); }
    if (s1.charAt(0) != s4) { WScript.Echo("fail"); } else { WScript.Echo("pass"); }
    if (s1.charAt(0) !== s4) { WScript.Echo("fail"); } else { WScript.Echo("pass"); }
    if (s1.charAt(0) == s5) { WScript.Echo("fail"); } else { WScript.Echo("pass"); }
    if (s1.charAt(0) === s5) { WScript.Echo("fail"); } else { WScript.Echo("pass"); }
    if (s1.charAt(0) != s5) { WScript.Echo("pass"); } else { WScript.Echo("fail"); }
    if (s1.charAt(0) !== s5) { WScript.Echo("pass"); } else { WScript.Echo("fail"); }
    if (s1 == s6) { WScript.Echo("fail"); } else { WScript.Echo("pass"); }
    if (s1 === s6) { WScript.Echo("fail"); } else { WScript.Echo("pass"); }
    if (s1 != s6) { WScript.Echo("pass"); } else { WScript.Echo("fail"); }
    if (s1 !== s6) { WScript.Echo("pass"); } else { WScript.Echo("fail"); }
    if (s7 == s1) { WScript.Echo("fail"); } else { WScript.Echo("pass"); }
    if (s7 === s1) { WScript.Echo("fail"); } else { WScript.Echo("pass"); }
    if (s7 != s1) { WScript.Echo("pass"); } else { WScript.Echo("fail"); }
    if (s7 !== s1) { WScript.Echo("pass"); } else { WScript.Echo("fail"); }
    if (s7 == s8) { WScript.Echo("pass"); } else { WScript.Echo("fail"); }
    if (s7 === s8) { WScript.Echo("pass"); } else { WScript.Echo("fail"); }
    if (s7 != s8) { WScript.Echo("fail"); } else { WScript.Echo("pass"); }
    if (s7 !== s8) { WScript.Echo("fail"); } else { WScript.Echo("pass"); }
}

var s1 = "This is a string";
var s2 = "This is another string";
var s3 = "This is a string";
var s4 = "T";
var s5 = "X";
var s6 = { };
var s7 = s1.slice(-1,0);
var s8 = "";

test(s1, s2, s3, s4, s5, s6, s7, s8);