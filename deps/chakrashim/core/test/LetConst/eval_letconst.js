//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

let x = 2;
let y = 1;
function blah()
{
    return 3;
}

function __parseUri() {
    var c = __parseUri;
    if (y == 1)
        WScript.Echo(c.options);
}

__parseUri.options = "Pass"; 

__parseUri();
