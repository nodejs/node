//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function func()
{
    WScript.Echo(i);
}

var i;
i = 0; // dead store
i = 1;
WScript.Echo(i);

i = 0;  // no deadstore
func();
i = 1;
WScript.Echo(i);

i = 0;  // no deadstore
var obj = this;
var j = obj.i;
obj.i = -1;
i = 1;  // no deadstore
WScript.Echo(i);
