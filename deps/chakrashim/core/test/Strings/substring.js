//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Testing our implementation of the substring, making sure that we keep the correct reference
// of the original string so that it doesn't get collected
var s = new String();
s = s + 'a';
s = s + 'b';
s = s + 'c';
s = s + 'd';
s = s + 'e';
var s1 = s.substr(1,3);
var s2 = s1.substr(1,2);
s = undefined;
s1 = undefined;

//implicit calls
function foo()
{
    var a = 1;
    var b = 2;
    var obj = {toString: function(){ a=3; return "Hello World";}};
    a = b;
    Object.prototype.substring = String.prototype.substring;
    var f = obj.substring(2,7);
    WScript.Echo (a);
}
foo();

CollectGarbage();
WScript.Echo(s2);
