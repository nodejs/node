//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------



var re1 = /^\s{2,}|\s{3,}$/g;
WScript.Echo("Test 1.1");
WScript.Echo("     blah     ".replace(re1,'') + "<END>");
WScript.Echo("====================");
WScript.Echo("Test 1.2");
WScript.Echo("     blah  ".replace(re1,'') + "<END>");
WScript.Echo("====================");
WScript.Echo("Test 1.3");
WScript.Echo(" blah   ".replace(re1,'') + "<END>");



var str1 = "        blah \n   blah   \n     blah\n     blah ";
var str2 = "     \n   \n     \n    ";
var str3 = "     \nb   \nb     \n    b\n \n\n    ";
var str3 = "     \nb   \nb     \n    ";

var re2 = /^\s{5,}|\s{1,}$/gm;
WScript.Echo("====================");
WScript.Echo("Test 2.1 (Multiline)");
WScript.Echo(str1.replace(re2,'<E>') + "<END>");
WScript.Echo("====================");
WScript.Echo("Test 2.2 (Multiline)");
WScript.Echo(str2.replace(re2, '<E>') + "<END>");
WScript.Echo("====================");
WScript.Echo("Test 2.3 (Multiline)");
WScript.Echo(str3.replace(re2, '<E>') + "<END>");


var re3 = /^\s*|\s*$/gm;
WScript.Echo("====================");
WScript.Echo("Test 3.1 (Multiline)");
WScript.Echo(str1.replace(re3,'<E>') + "<END>");
WScript.Echo("====================");
WScript.Echo("Test 3.2 (Multiline)");
WScript.Echo(str2.replace(re3, '<E>') + "<END>");
WScript.Echo("====================");
WScript.Echo("Test 3.3 (Multiline)");
WScript.Echo(str3.replace(re3, '<E>') + "<END>");



