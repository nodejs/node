//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Bug 212755

function test1() {
    "some directive"+"";
    "use strict";

    with ({ }) { } // should be no error
}
test1();

function test2() {
    function deferred() {
        "some directive";
        "use strict";

        with ({ }) { } // should be an error in deferred parsing (as well as normal parsing of course)
    }
    deferred();
}
test2();

WScript.Echo("FAILED"); // should get a parse error in deferred() above
