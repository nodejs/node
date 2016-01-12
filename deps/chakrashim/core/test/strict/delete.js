//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo() {
    "use strict";
    var a;
    delete (a); // Win8 776066: delete (identifier) should result in SyntaxError in strict mode.
}

foo();
