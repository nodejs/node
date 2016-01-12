//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var arr=new Array(1,2,3,4,5,6);
WScript.Echo(arr);
var newarr = Array.apply(this, arr);
WScript.Echo(newarr);