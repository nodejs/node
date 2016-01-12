//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// OS5403724: Inlined ES6 class constructor not getting new.target",
//  -maxinterpretcount:3 -off:simplejit

var A = class {
    constructor () { }
}

for (var i=0; i<4; i++)
{
    (()=>new A())();
}

print("pass");
