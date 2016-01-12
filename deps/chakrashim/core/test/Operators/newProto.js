//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function Blah()
{
    this.hello = "yay";
}

Blah.prototype = "meow"

function Derived()
{
}

Derived.prototype = new Blah();

var blah = new Blah();
WScript.Echo(blah.hello);
WScript.Echo(blah.toString());
var derived = new Derived();
WScript.Echo(derived.toString());
