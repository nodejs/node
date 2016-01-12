//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var x= { f: function() { return "hello"; } }
WScript.Echo(x.propertyIsEnumerable("f"));
WScript.Echo(x.propertyIsEnumerable("g"));
var e=new Array();
WScript.Echo(Array.propertyIsEnumerable("length"));
WScript.Echo(e.propertyIsEnumerable("length"));


