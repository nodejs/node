//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

if (this.WScript && this.WScript.LoadScriptFile) {
    this.WScript.LoadScriptFile("TrimStackTracePath.js");
}

function bar(a)
{
    for(var i = 0; i<4; i++)
    {
        for(var j = 0; j<4; j++)
        {
            try
            {
                baz();
            }
            catch(ex)
            {
                WScript.Echo(TrimStackTracePath(ex.stack));
            }
        }
    }
}
bar(1);
