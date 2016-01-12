//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

var obj = [];
var proto = [];
var count = 2;

for(var i = 0; i < count; ++i)
{
    obj[i] = (new (g=function f() { this.x=3; })());
    proto[i] = g;
}

for(var i = 0; i < count; ++i)
{
    write("Testing object " + i + "............");

    for(var j = 0; j < count; ++j)
    {
        write("obj[" + i + "] instanceof proto[" + j + "] : " + (obj[i] instanceof proto[j]));
    }
}

proto[0].prototype.z = "proto[0].z";
proto[0].prototype.w = "proto[0].w";

write("Checking properties .........");
for(var i = 0; i < count; ++i)
{
    write("obj[" + i + "].z : " + obj[i].z);
    write("obj[" + i + "].w : " + obj[i].w);
}

var a = function x() {
    function foo() {
        "use strict";
        x = 1;
    };
}

