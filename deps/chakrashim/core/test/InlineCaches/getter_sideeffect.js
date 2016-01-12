//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function Ctor()
{
};

Object.defineProperty(Ctor.prototype, "x", { get: function() { this.count = 0; return 1; } });

function f(o)
{
    return o.x;
}

if (f(new Ctor()) == f(new Ctor())) { WScript.Echo("PASS"); }

