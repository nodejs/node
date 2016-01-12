//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var test = function() {
    function getArgs() {
        return arguments;
    }
    var args = getArgs(2, 3);
    if(args.length === 2)
    {
        WScript.Echo("PASSED");
    }
    else
    {
        WScript.Echo(args.length);
    }
}
test();
