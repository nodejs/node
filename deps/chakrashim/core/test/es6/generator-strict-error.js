//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


try
{
    eval("function test() { function * arguments() { \"use strict\"; } }; test();");
}
catch (e)
{
    WScript.Echo(e);
}

