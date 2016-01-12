//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function MyReplace($0, $1, $2, $3, $4, offset, input)
{
    WScript.Echo("$0=" + $0);
    WScript.Echo("$1=" + $1);
    WScript.Echo("$2=" + $2);
    WScript.Echo("$3=" + $3);
    WScript.Echo("$4=" + $4);
    WScript.Echo("offset=" + offset);
    WScript.Echo("input=" + input);
    return $0;
}

var p = /(a)(b)(c)(d)/g;
var s = "xxabcdxxabcdxx";

WScript.Echo(s.replace(p, MyReplace));


var replacefn = function (arg1,arg2,arg3)
{
 this.x = 10; 
 return "xyz";
}

var a = new String("abcdef");
WScript.Echo(a.replace("def",replacefn));
WScript.Echo(x);


replacefn = function(arg) {
    // access re.lastIndex inside replace function.
    // As per ES6 21.2.5.8, lastIndex should be updated to 0 if global is true
    // This should be visible in replace function
    WScript.Echo(re.lastIndex);
    return "_" + arg;
}
var re = /abc/g;
var str = "abcabc";
re.lastIndex = 3;
WScript.Echo(str.replace(re, replacefn));

