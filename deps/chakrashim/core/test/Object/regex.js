//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function hello()
{
    var regex = /blah/;
    WScript.Echo("blah: " + regex.blah);
    regex.blah = 1;
    WScript.Echo("blah: " + regex.blah);
}

hello();
hello();
