//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


var str = "a b\nc d\ne f";
WScript.Echo(str.replace(/^a/g, "replaced"));
WScript.Echo();
WScript.Echo(str.replace(/^a/gm, "replaced"));
WScript.Echo();
WScript.Echo(str.replace(/b$/g, "replaced"));
WScript.Echo();
WScript.Echo(str.replace(/b$/gm, "replaced"));
WScript.Echo();
WScript.Echo(str.replace(/^c d$/g, "replaced"));
WScript.Echo();
WScript.Echo(str.replace(/^c d$/gm, "replaced"));
WScript.Echo();
WScript.Echo(str.replace(/^e/g, "replaced"));
WScript.Echo();
WScript.Echo(str.replace(/^e/gm, "replaced"));
WScript.Echo();
WScript.Echo(str.replace(/f$/g, "replaced"));
WScript.Echo();
WScript.Echo(str.replace(/f$/gm, "replaced"));

