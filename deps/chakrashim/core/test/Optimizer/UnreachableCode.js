//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    throw eval('2251799813685249++');
    while(false);
}
safeCall(test0);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

function safeCall(f) {
    var args = [];
    for(var a = 1; a < arguments.length; ++a)
        args.push(arguments[a]);
    try {
        return f.apply(this, args);
    } catch(ex) {
        WScript.Echo(ex.name + ": " + ex.message);
    }
}
