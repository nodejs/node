//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.RegisterCrossThreadInterfacePS();
//
// data for "typeoftest.js"
//

var g_undefined = undefined;
var g_boolean   = true;
var g_number    = 12.34;
var g_string    = "a string";
var g_function  = function () { };
var g_object    = {};
var g_null      = null;

var g_data = [
    g_undefined,
    g_boolean  ,
    g_number   ,
    g_string   ,
    g_function ,
    g_object   ,
    g_null     ,
];
