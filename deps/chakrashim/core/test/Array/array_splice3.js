//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var echo = WScript.Echo;

// no argument
var a = [0, 1, 2, 3, 4];
echo("splice no arg:", a, "||", a.splice());

// "start" only
var starts = [-2, 0, 2, 8];
for (var i = 0; i < starts.length; i++) {
    var a = [0, 1, 2, 3, 4];
    var start = starts[i];
    echo("splice at " + start + ":", a, "||", a.splice(start));
}
